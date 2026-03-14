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
#include <QCheckBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QComboBox>

void MainWindow::copyQueueToSeries(const std::queue<double>& dataQueue,
                           const std::queue<double>& timeQueue,
                           QLineSeries* series,
                           const double start_plot_time,
                           const double end_plot_time)
{
    std::queue<double> temp_data = dataQueue;  // copy queue
    std::queue<double> temp_time = timeQueue;  // copy queue

    series->clear();  // optional: clear previous data

    int index = 0;

    // Append data to the series from queue every 50 times, and delete data from the queue every step
    while (!temp_data.empty())
    {
        double curr_time = temp_time.front();
        if (index % plot_data_freq == 0 && 
            curr_time > start_plot_time && 
            curr_time < end_plot_time) {
            series->append(temp_time.front(), temp_data.front());
        } else if (curr_time == end_plot_time) {
            return;
        }
        temp_data.pop();
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

    startButton = new QPushButton("Start Plot");
    startButton->setCheckable(true);

    //Create Export Button
    exportButton = new QPushButton("Export CSV");

    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // Create chart
    for (int i = 0; i < series_names.size(); ++i)
    {
        QChart* chart = new QChart();
        chart->setTitle(series_names[i]);

        QLineSeries* s = new QLineSeries();
        s->setName(series_names[i]);

        chart->addSeries(s);

        // Create axes
        QValueAxis* ax = new QValueAxis;
        ax->setTitleText("Time (s)");
        ax->setLabelFormat("%.2f");
        ax->setRange(0, WINDOW); //Initial Range

        QValueAxis* ay = new QValueAxis;
        ay->setTitleText(series_names[i]);
        ay->setRange(-500,500);
        ay->setTickInterval(100);
        ay->setMinorTickCount(4);

        //Add axes to chart and attach series
        chart->addAxis(ax, Qt::AlignBottom);
        chart->addAxis(ay, Qt::AlignLeft);

        s->attachAxis(ax);
        s->attachAxis(ay);

        charts.push_back(chart);
        axisX.push_back(ax);
        axisY.push_back(ay);

        series.push_back({s});   // series[chart][line]
    }

    //Initialize joint_series_buffers and session_log_series
    for (int j = 0; j < NUM_JOINTS; ++j) {
        joint_series_buffers[j].resize(series_names.size());
    }

    session_log_series.resize(series_names.size());

    //Create chart view (widget that displays chart)
    for (auto chart : charts)
    {
        QChartView* view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);
        view->setRubberBand(QChartView::RectangleRubberBand);

        chartViews.push_back(view);
        layout->addWidget(view);
    }

    //Set chartView as the central widget of main window
    setCentralWidget(central);

    // Timer at 1 Hz (do not start yet)
    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer); // This is the secret sauce
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePlot);
    //timer->start(1000);

    // Connect start Button
    connect(startButton, &QPushButton::clicked, this, &MainWindow::togglePlot);


    // Create the Input box for x -axis initial range ----------------------------------------------------------------

    QHBoxLayout *controlLayout = new QHBoxLayout;

    QLabel *label = new QLabel("Time Window (sec):");
    windowEdit = new QLineEdit("10");
    windowEdit->setFixedWidth(80);

    controlLayout->addWidget(label);
    controlLayout->addWidget(windowEdit);
    controlLayout->addStretch();

    //layout->addLayout(controlLayout);

    connect(windowEdit, &QLineEdit::editingFinished, this, [=]()
        {
            bool ok;
            double value = windowEdit->text().toDouble(&ok);

            if (ok && value > 0)
            {
                WINDOW = value;
            }
            else
            {
                windowEdit->setText(QString::number(WINDOW));
            }
        });

    
    // Add Auto-scale check-box for Y - axis
    autoScaleCheck = new QCheckBox("Auto Scale Y");
    autoScaleCheck->setChecked(true);
    controlLayout->addWidget(autoScaleCheck);

    connect(exportButton, &QPushButton::clicked,
        this, &MainWindow::exportCSV);

    // Create a dropdown to select the joint you want to plot
    jointSelector = new QComboBox(this);
    controlLayout->addWidget(new QLabel("Select Joint:"));
    controlLayout->addWidget(jointSelector);

    layout->addLayout(controlLayout);

    // Connect selection change
    connect(jointSelector,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int){
                if (jointSelector->count() == 0)
                    return;
                selected_joint = jointSelector->currentData().toInt();
            });
    
    // Connect shared memory to show active joints
    connectSharedMemory();
    // Update joint selection dropdown based on active joints
        uint8_t mask = shm_ptr->buffer[(shm_ptr->write_index-1) % BUFFER_SIZE].active_joint_mask;
        qDebug() << "Active mask:" << mask;
        if (mask != current_mask) {
            jointSelector->clear();
            for (int j = 0; j < NUM_JOINTS; j++) {
                if (mask & (1 << j)) {
                    jointSelector->addItem(QString("Joint %1").arg(j+1), j);
                }
            }
            current_mask = mask;

            if (jointSelector->count() > 0){
                selected_joint = jointSelector->currentData().toInt();
            }
        }
        if (!shm_ptr) {
            qDebug() << "Shared memory not available!";
            return;
        }
    
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
        //connectSharedMemory();

        if (!shm_ptr) {
            qDebug() << "Shared memory not available!";
            startButton->setChecked(false);
            return;
        }

        last_index = shm_ptr->write_index;
        //qDebug() << "last_index: " << last_index;
        first_timestamp = 0;

        timer->start(plot_refresh_freq);  // adjust plot refresh frequency
        startButton->setText("Stop Torque Plot");

        // Clear previous session (WHEN START IS PRESSED)
        session_time_log.clear();
        for (auto &vec : session_log_series) vec.clear();

        isLogging = true;

    } else {
        qDebug() << "Plotting stopped";
        timer->stop();
        startButton->setText("Start Torque Plot");

        //Clear PLot on Stop
    for (auto &chart_series : series)
    {
        for (auto s : chart_series){
            s->clear();
        }
    }

        //Sync index to latest controller position
        last_index = shm_ptr->write_index;

        //Reset time reference
        first_timestamp = 0;

        // Clear buffers
        while (!time_buffer.empty()) time_buffer.pop();
        for (int j = 0; j < NUM_JOINTS; j++) {
            for (int k = 0; k < series_names.size(); ++k) {
                while (!joint_series_buffers[j][k].empty()) joint_series_buffers[j][k].pop();
            }
        }
        //WHEN STOP IS PRESSED
        isLogging = false;
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

        // torqueActualSeries->append(last_time_sec, s.torque_actual);
        time_buffer.push(last_time_sec);

        for (int j = 0; j < NUM_JOINTS; j++) {
            if (s.active_joint_mask & (1 << j)) {
                joint_series_buffers[j][0].push(s.torque_actual[j]);
                joint_series_buffers[j][1].push(s.torque_cmd[j]);
                for (int k = 0; k < series_names.size(); ++k) {
                    if (joint_series_buffers[j][k].size() > BUFFER_SIZE){
                        joint_series_buffers[j][k].pop();
                    }
                }
            }
        }
        if (time_buffer.size() > BUFFER_SIZE){
            time_buffer.pop();
        }

        last_index++;

        //Log data into memory while plotting
        if (isLogging){
            session_time_log.push_back(last_time_sec);
            for (int k = 0; k < series_names.size(); ++k) {
                session_log_series[k].push_back(joint_series_buffers[selected_joint][k].back());
            }
        }
    
    }

    // transfer torque buffer to torqueActualSeries
    last_time_sec = time_buffer.back();
    for (int k = 0; k < series_names.size(); ++k)
        {
            copyQueueToSeries(joint_series_buffers[selected_joint][k], time_buffer, series[k][0], last_time_sec - WINDOW, last_time_sec);
        }
    for (int k = 0; k < series_names.size(); ++k){
        if (series[k][0]->count() == 0)
        return;
    }
    

    // ----------------------------
    //  AXIS BEHAVIOR
    // ----------------------------

    for (int i = 0; i < axisX.size(); ++i)
    {
        if (last_time_sec <= WINDOW)
            // First 10 seconds → fixed axis
            axisX[i]->setRange(0, WINDOW);
        else
            // After 10 seconds → sliding window
            axisX[i]->setRange(last_time_sec - WINDOW, last_time_sec);
    }

    // AutoScale the Y-Axis
    if (autoScaleCheck->isChecked()) {
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxY = std::numeric_limits<qreal>::lowest();
        for (auto &chart_series : series){
            for (auto s : chart_series) {
                const auto points = s->points();
                for (const QPointF &p : points) {
                    minY = std::min(minY, p.y());
                    maxY = std::max(maxY, p.y());
                }
            }
        }
        qreal padding = (maxY - minY) * 0.1;
        if (padding == 0) padding = 1.0;{
            for (auto ay : axisY)
            {
                ay->setRange(minY - padding, maxY + padding);
            }
        }
    }
}

//Implement export from the session log
void MainWindow::exportCSV()
{
    if (session_time_log.empty())
        return;

    // Get current working directory
    QDir dir (QDir::currentPath());

    //Create logs folder if it doesn't exist
    if (!dir.exists("logs")){
        dir.mkdir("logs");
    }

    //Go into logs folder
    dir.cd("logs");

    QString fileName = "joint_" + QString::number(selected_joint+1) + "torque_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") +
        ".csv";


    QString fullPath = dir.filePath(fileName);

    QFile file(fullPath);
    if (fileName.isEmpty())
        return;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);

    // CSV header
    out << "Time (s),Joint";
    for (const auto& name : series_names) out << "," << name;
    out << "\n";

    // Each row
    for (size_t i = 0; i < session_time_log.size(); ++i)
    {
        out << session_time_log[i] << "," << (selected_joint+1);
        for (size_t k = 0; k < session_log_series.size(); ++k)
        {
            out << "," << session_log_series[k][i];
        }
        out << "\n";
    }

    file.close();
}


MainWindow::~MainWindow()
{
    delete ui;
}