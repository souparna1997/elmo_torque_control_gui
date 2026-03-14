#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>  //Base class for main windows
#include <QtCharts/QChartView> //Widget to display charts
#include <QtCharts/QLineSeries> //Line series to store points (x, y)
#include <QtCharts/QValueAxis> //Numeric axis(x/y)
#include <QTimer> //Timer for periodic updates
#include "shared_data.h" // Single Producer Single Consumer Circular Buffer
#include <QVBoxLayout>
#include <QPushButton>
#include <queue>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QComboBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updatePlot();
    void togglePlot();
    void copyQueueToSeries(const std::queue<double>& torqueQueue,
                           const std::queue<double>& timeQueue,
                           QLineSeries* torqueSeries,
                           const double start_plot_time = 0,
                           const double end_plot_time = 10);
    void exportCSV();

private:
    Ui::MainWindow *ui;

    std::vector<QChart*> charts;
    std::vector<QChartView*> chartViews;
    std::vector<std::vector<QLineSeries*>> series;
    std::vector<QString> series_names = {
        "Torque Actual",
        "Torque Command"
        //Add more later
    };
    QTimer *timer;
    QLineEdit *windowEdit;
    QCheckBox *autoScaleCheck;
    QComboBox* jointSelector; // Dropdown to select joint

    SharedMemory *shm_ptr; // pointer to the shared memory (read-only on GUI side)
    uint32_t last_index; // Tracks the last sample that has been plotted

    uint64_t first_timestamp = 0; // member variable (Used to normalize the first sample so that the X-axis starts at 0)
    double time_sec = 0.0;   // member variable
    double WINDOW = 10.0;   // 10-second visible window
    int plot_refresh_freq = 20; // Plot refresh frequency set to 50 Hz
    const int plot_data_freq = 10; // Set plot data frequency
    uint8_t current_mask = 0;
    uint8_t selected_joint = 0; // currently selected joint


    std::vector<QValueAxis*> axisX;
    std::vector<QValueAxis*> axisY;

    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *exportButton;

    // Per joint per series buffer
    std::vector<std::queue<double>> joint_series_buffers[NUM_JOINTS];
    std::queue<double> time_buffer;

    // Logging session dat for export
    std::vector<double> session_time_log;
    std::vector<std::vector<double>> session_log_series;

    bool isLogging = false;

    void connectSharedMemory();
};
#endif // MAINWINDOW_H