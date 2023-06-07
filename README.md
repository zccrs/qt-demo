# Example

````
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
````

The output:

````
void test_void() QThread(0x7ffe8cd86900, name = "TestThread")
void test_void_int(int) QThread(0x7ffe8cd86900, name = "TestThread") 1
int test_int_init(int) QThread(0x7ffe8cd86900, name = "TestThread") 2
QVariant test_variant(QVariant) QThread(0x7ffe8cd86900, name = "TestThread") QVariant(QString, "string")
int test_ensure_call_in_gui_thread(int) QThread(0x7ffe8cd86900, name = "TestThread") 3
QThread(0x55cf5024f690, name = "MainThread") QVariant(QThread*, 0x7ffe8cd86900)
int test_int_init(int) QThread(0x55cf5024f690, name = "MainThread") 3
````
