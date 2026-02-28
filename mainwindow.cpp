#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "shared_data.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    shm_ptr(nullptr),
    last_index(0)
{
    ui->setupUi(this);

    // Create plot series
    torqueSeries = new QLineSeries();

    // Create chart and attach series
    chart = new QChart();
    chart->addSeries(torqueSeries);
    chart->setTitle("Motor Torque (Actual)");

    // Connect shared memory
    connectSharedMemory();
    if (!shm_ptr) qDebug() << "Shared memory not connected!";

    // Create axes
    axisX = new QValueAxis;
    axisX->setTitleText("Time (s)");
    axisX->setLabelFormat("%.2f");

    axisY = new QValueAxis;
    axisY->setTitleText("Torque (Actual)");
    axisY->setLabelFormat("%d");
    axisY->setRange(-500, 500);
    axisY->setTickInterval(100);
    axisY->setMinorTickCount(4);

    //Add axes to chart and attach series
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    torqueSeries->attachAxis(axisX);
    torqueSeries->attachAxis(axisY);

    //Create chart view (widget that displays chart)
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    //Set chartView as the central widget of main window
    setCentralWidget(chartView);

    // Timer at 50 Hz
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer); // This is the secret sauce
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePlot);
    timer->start(1000);
}

void MainWindow::connectSharedMemory()
{
    int fd = shm_open("/motor_shm", O_RDWR, 0666);

    if (fd < 0) {
        qDebug("Failed to open shared memory!");
        return;
    }

    shm_ptr = (SharedMemory*)mmap(
        nullptr,
        sizeof(SharedMemory),
        PROT_READ,
        MAP_SHARED,
        fd,
        0);
}

void MainWindow::updatePlot()
{
    if (!shm_ptr) return;

    uint32_t current_index = shm_ptr->write_index;
    if (current_index == 0) return;  // no data yet

    double last_time_sec = 0.0;

    // Append all new samples
    while (last_index < current_index)
    {
        MotorSample &s = shm_ptr->buffer[last_index % BUFFER_SIZE];

        if (first_timestamp == 0)
            first_timestamp = s.timestamp;

        last_time_sec = (s.timestamp - first_timestamp) * 1e-9;

        torqueSeries->append(last_time_sec, s.torque_actual);

        // qDebug() << "Timestamp:" << s.timestamp
        //          << "Torque:" << s.torque_actual;

        last_index++;
    }

    // Keep last 5 seconds visible
    axisX->setRange(last_time_sec - 5, last_time_sec);

    // Limit number of points to prevent slowdown
    if (torqueSeries->count() > 1000)
        torqueSeries->removePoints(0, torqueSeries->count() - 1000);

    // Force chart redraw
    //chart->update();
}

MainWindow::~MainWindow()
{
    delete ui;
}