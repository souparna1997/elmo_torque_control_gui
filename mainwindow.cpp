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

void MainWindow::copyQueueToSeries(const std::queue<double>& torqueQueue,
                           const std::queue<double>& timeQueue,
                           QLineSeries* torqueSeries,
                           const double start_plot_time,
                           const double end_plot_time)
{
    std::queue<double> temp_torque = torqueQueue;  // copy queue
    std::queue<double> temp_time = timeQueue;  // copy queue

    torqueSeries->clear();  // optional: clear previous data

    int index = 0;

    // Append data to the series from queue every 50 times, and delete data from the queue every step
    while (!temp_torque.empty())
    {
        double curr_time = temp_time.front();
        if (index % plot_data_freq == 0 && 
            curr_time > start_plot_time && 
            curr_time < end_plot_time) {
            torqueSeries->append(temp_time.front(), temp_torque.front());
        } else if (curr_time == end_plot_time) {
            return;
        }
        temp_torque.pop();
        temp_time.pop();
        index++;
    }
}

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
    axisX->setRange(0, 10); //Initial Range

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

        timer->start(plot_refresh_freq);  // adjust plot refresh frequency
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

    uint32_t current_index = shm_ptr->write_index;
    if (current_index == 0) return;

    double last_time_sec = 0.0;

    while (last_index < current_index)
    {
        MotorSample &s = shm_ptr->buffer[last_index % BUFFER_SIZE];

        if (first_timestamp == 0)
            first_timestamp = s.timestamp;

        last_time_sec = (s.timestamp - first_timestamp) * 1e-9;

        // torqueSeries->append(last_time_sec, s.torque_actual);
        time_buffer.push(last_time_sec);
        torque_buffer.push(s.torque_actual);

        if (torque_buffer.size() > BUFFER_SIZE) {
            time_buffer.pop();
            torque_buffer.pop();
        }

        last_index++;
    }

    // transfer torque buffer to torqueSeries
    last_time_sec = time_buffer.back();
    copyQueueToSeries(torque_buffer, time_buffer, torqueSeries, last_time_sec - WINDOW, last_time_sec);

    if (torqueSeries->count() == 0)
        return;

    // ----------------------------
    //  AXIS BEHAVIOR
    // ----------------------------
    if (last_time_sec <= WINDOW)
    {
        // First 10 seconds → fixed axis
        axisX->setRange(0, WINDOW);
    }
    else
    {
        // After 10 seconds → sliding window
        axisX->setRange(last_time_sec - WINDOW, last_time_sec);
    }
}


MainWindow::~MainWindow()
{
    delete ui;
}