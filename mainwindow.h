#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>  //Base class for main windows
#include <QtCharts/QChartView> //Widget to display charts
#include <QtCharts/QLineSeries> //Line series to store points (x, y)
#include <QtCharts/QValueAxis> //Numeric axis(x/y)
#include <QTimer> //Timer for periodic updates
#include "shared_data.h" // Single Producer Single Consumer Circular Buffer

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

private:
    Ui::MainWindow *ui;

    QChart *chart;
    QLineSeries *torqueSeries;
    QChartView *chartView;
    QTimer *timer;

    SharedMemory *shm_ptr;
    uint32_t last_index;

    uint64_t first_timestamp = 0; // member variable
    double time_sec = 0.0;   // member variable

    QValueAxis *axisX;
    QValueAxis *axisY;

    void connectSharedMemory();
};
#endif // MAINWINDOW_H
