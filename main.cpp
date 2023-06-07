#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QFuture>
#include <QVariant>

namespace QtConcurrent {
    static QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::registerEventType());

    template <typename Fun, typename... Args>
    class _Caller : public QObject
    {
        typedef QtPrivate::FunctionPointer<std::decay_t<Fun>> FunInfo;
        typedef std::invoke_result_t<std::decay_t<Fun>, Args...> ReturnType;

        class CallEvent : public QEvent {
        public:
            CallEvent(QEvent::Type type, Fun &&fun, Args&&... args)
                : QEvent(type)
                , function(fun)
                , arguments(std::forward<Args>(args)...)
            {

            }

            QEvent *clone() const override {
                return nullptr;
            }

            Fun function;
            const std::tuple<Args...> arguments;
            QPromise<ReturnType> promise;

            QObject *condition;
            QPointer<QObject> conditionRef;
        };

    public:
        explicit _Caller(QThread *thread)
            : QObject()
        {
            moveToThread(thread);
        }

        static QFuture<ReturnType> run(QThread *thread, QObject *condition, Fun fun, Args&&... args)
        {
            if constexpr (FunInfo::IsPointerToMemberFunction) {
                static_assert(std::is_same_v<std::decay_t<typename QtPrivate::List<Args...>::Car>, typename FunInfo::Object>,
                              "The obj and function are not compatible.");
                static_assert(QtPrivate::CheckCompatibleArguments<typename QtPrivate::List<Args...>::Cdr, typename FunInfo::Arguments>::value,
                              "The args and function are not compatible.");
            } else if constexpr (FunInfo::ArgumentCount != -1) {
                static_assert(QtPrivate::CheckCompatibleArguments<QtPrivate::List<Args...>, typename FunInfo::Arguments>::value,
                              "The args and function are not compatible.");
            } else {
                using Prototype = ReturnType(*)(Args...);
                QtPrivate::AssertCompatibleFunctions<Prototype, Fun>();
            }

            QPromise<ReturnType> promise;
            auto future = promise.future();

            if (Q_UNLIKELY(QThread::currentThread() == thread)) {
                promise.start();
                if constexpr (std::is_void<ReturnType>::value) {
                    std::invoke(fun, std::forward<Args>(args)...);
                } else {
                    promise.addResult(std::invoke(fun, std::forward<Args>(args)...));
                }
                promise.finish();
            } else {
                CallEvent *event = new CallEvent(eventType, std::move(fun), std::forward<Args>(args)...);
                event->promise = std::move(promise);
                event->condition = condition;
                event->conditionRef = condition;

                _Caller *receiver = new _Caller(thread); // TODO: Don't create a new Caller for the same threads.
                QCoreApplication::postEvent(receiver, event);
            }

            return future;
        }

        bool event(QEvent *event) override
        {
            if (event->type() == eventType) {
                auto ev = static_cast<CallEvent*>(event);
                if (ev->conditionRef == ev->condition) {
                    ev->promise.start();
                    if constexpr (std::is_void<ReturnType>::value) {
                        std::apply(ev->function, ev->arguments);
                    } else {
                        ev->promise.addResult(std::apply(ev->function, ev->arguments));
                    }
                    ev->promise.finish();
                } else {
                    ev->promise.finish();
                }
                this->deleteLater();
                return true;
            }

            return QObject::event(event);
        }
    };

    template <typename Fun, typename... Args>
    inline auto privateCall(QThread *thread, QObject *condition, Fun fun, Args&&... args) {
        return _Caller<Fun, Args...>::run(thread, condition, fun, std::forward<Args>(args)...);
    }

    template <typename Fun, typename... Args>
    inline auto run(QThread *thread, QObject *condition, typename QtPrivate::FunctionPointer<Fun>::Object *obj, Fun fun, Args&&... args)
    {
        return privateCall(thread, condition, fun, *obj, std::forward<Args>(args)...);
    }
    template <typename Fun, typename... Args>
    inline auto run(QThread *thread, typename QtPrivate::FunctionPointer<Fun>::Object *obj, Fun fun, Args&&... args)
    {
        if constexpr (std::is_base_of<QObject, typename QtPrivate::FunctionPointer<Fun>::Object>::value) {
            return privateCall(thread, obj, fun, *obj, std::forward<Args>(args)...);
        } else {
            return privateCall(thread, static_cast<QObject*>(nullptr), fun, *obj, std::forward<Args>(args)...);
        }
    }
    template <typename Fun, typename... Args>
    inline QFuture<std::invoke_result_t<std::decay_t<Fun>, Args...>>
    run(QThread *thread, QObject *condition, Fun fun, Args&&... args)
    {
        return privateCall(thread, condition, fun, std::forward<Args>(args)...);
    }
    template <typename Fun, typename... Args>
    inline QFuture<std::invoke_result_t<std::decay_t<Fun>, Args...>>
    run(QThread *thread, Fun fun, Args&&... args)
    {
        return privateCall(thread, static_cast<QObject*>(nullptr), fun, std::forward<Args>(args)...);
    }
    template <typename... T> inline auto
    exec(QThread *thread, T&&... args)
    {
        auto future = run(thread, std::forward<T>(args)...);
        if (!thread->isRunning()) {
            qWarning() << "The target thread is not running, maybe lead to deadlock.";
        }
        future.waitForFinished();

        if constexpr (std::is_same_v<decltype(future), QFuture<void>>) {
            return;
        } else {
            return future.result();
        }
    }
}

void test_void() {
    qDebug() << Q_FUNC_INFO << QThread::currentThread();
}

void test_void_int(int v) {
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << v;
}

int test_int_init(int v) {
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << v;
    return v;
}

QVariant test_variant(QVariant v) {
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << v;
    return QVariant::fromValue(QThread::currentThread());
}

int test_ensure_call_in_gui_thread(int v) {
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << v;
    return QtConcurrent::exec(QCoreApplication::instance()->thread(), test_int_init, v);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    a.thread()->setObjectName("MainThread");

    QThread testThread;
    testThread.setObjectName("TestThread");
    testThread.start();

    QtConcurrent::run(&testThread, test_void);
    QtConcurrent::run(&testThread, test_void_int, 1);
    QtConcurrent::run(&testThread, test_int_init, 2);

    auto future = QtConcurrent::run(&testThread, test_variant, QString("string"));
    future.then(&a, [] (QVariant v) {
        qDebug() << QThread::currentThread() << v;
    });

    QtConcurrent::run(&testThread, test_ensure_call_in_gui_thread, 3);

    return a.exec();
}
