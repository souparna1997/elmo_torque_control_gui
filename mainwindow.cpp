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
#include <QVBoxLayout>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) //Initialize base class
    , ui(new Ui::MainWindow), //Initialize UI Pointer
    shm_ptr(nullptr),
    last_index(0) //Initialize last index value to zero
    , first_timestamp(0)
{
    ui->setupUi(this); //Populate window with widgets from .ui

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    // Create Start and Stop Button
    QHBoxLayout *buttonLayout  = new QHBoxLayout;

    startButton = new QPushButton("Start Torque Plot");
    startButton->setCheckable(true);

    layout->addWidget(startButton);

    // Create line series for plotting
    torqueSeries = new QLineSeries();

    // Create chart and attach series
    chart = new QChart();
    chart->addSeries(torqueSeries);
    chart->setTitle("Motor Torque (Actual)");

    // Create axes
    axisX = new QValueAxis;
    axisX->setTitleText("Time (s)");
    axisX->setLabelFormat("%.2f");
    //axisX->setRange(0,10); //Initial Range

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

    layout->addWidget(chartView);

    //Set chartView as the central widget of main window
    setCentralWidget(central);

    // Timer at 1 Hz (do not start yet)
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer); // This is the secret sauce
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePlot);
    //timer->start(1000);

    // Connect start Button
    connect(startButton, &QPushButton::clicked, this, &MainWindow::togglePlot);
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

    ::close(fd);

    if (shm_ptr == MAP_FAILED)
    {
        qDebug("mmap failed!");
        shm_ptr = nullptr;
        return;
    }
    

}

void MainWindow::togglePlot()
{
    if (startButton->isChecked()) {
        qDebug() << "Plotting started";
        connectSharedMemory();

        if (!shm_ptr) {
            qDebug() << "Shared memory not available!";
            startButton->setChecked(false);
            return;
        }

        last_index = shm_ptr->write_index;
        first_timestamp = 0;

        timer->start(10);  // adjust frequency
        startButton->setText("Stop Torque Plot");

    } else {
        qDebug() << "Plotting stopped";
        timer->stop();
        startButton->setText("Start Torque Plot");

        //Clear PLot on Stop
        torqueSeries->clear();
        last_index = shm_ptr->write_index;
        first_timestamp = 0;
    }
}


void MainWindow::updatePlot()
{
    if (!shm_ptr) return;

    // Read the data from the buffer
    uint32_t current_index = shm_ptr->write_index; //current_index is total samples written by the controller so far
    if (current_index == 0) return;  // no data yet

    double last_time_sec = 0.0;

    // Append all new samples that haven't been plotted yet
    while (last_index < current_index)
    {
        // Fetch the sample from the circular buffer (%BUFFER_SIZE wraps around if the controller has overwritten old samples)
        MotorSample &s = shm_ptr->buffer[last_index % BUFFER_SIZE];

        //Record the timestamp for the VERY FIRST sample plotted (This becomes the time-origin x = 0 on the plot)
        if (first_timestamp == 0)
            first_timestamp = s.timestamp;

        last_time_sec = (s.timestamp - first_timestamp) * 1e-9; //Convert ns to s for plotting
        axisX->setRange(0, last_time_sec); // expand to include new points

        torqueSeries->append(last_time_sec, s.torque_actual); //Append the new data to the chart

        // qDebug() << "Timestamp:" << s.timestamp
        //          << "Torque:" << s.torque_actual;

        last_index++; //last index moves forward as we append samples
    }

    // Limit number of points to prevent slowdown
    if (torqueSeries->count() > 20000)
        torqueSeries->removePoints(0, torqueSeries->count() - 20000);

    // Force chart redraw
    //chart->update();
}


MainWindow::~MainWindow()
{
    delete ui;
}