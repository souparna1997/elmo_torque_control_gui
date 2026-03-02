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

private:
    Ui::MainWindow *ui;

    QChart *chart;
    QLineSeries *torqueSeries;
    QChartView *chartView;
    QTimer *timer;

    SharedMemory *shm_ptr; // pointer to the shared memory (read-only on GUI side)
    uint32_t last_index; // Tracks the last sample that has been plotted

    uint64_t first_timestamp = 0; // member variable (Used to normalize the first sample so that the X-axis starts at 0)
    double time_sec = 0.0;   // member variable

    QValueAxis *axisX;
    QValueAxis *axisY;

    QPushButton *startButton;
    QPushButton *stopButton;

    void connectSharedMemory();
};
#endif // MAINWINDOW_H
