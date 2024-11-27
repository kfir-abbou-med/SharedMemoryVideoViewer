#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <iostream>
#include <cstring>

using namespace boost::interprocess;
using namespace cv;
using namespace std;

class SharedMemoryViewer : public QMainWindow
{
public:
    SharedMemoryViewer(const std::string& shm_name, QWidget *parent = nullptr) : QMainWindow(parent), m_shmName(shm_name)
    {
        imageLabel = new QLabel(this);
        setCentralWidget(imageLabel);
        resize(640, 480);
        setWindowTitle(("Shared Memory Frame Viewer: " + shm_name).c_str());

        // Timer for frame update
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &SharedMemoryViewer::updateFrame);
        timer->start(33); // ~30 FPS
    }

private slots:
    void updateFrame()
    {
        try
        {
            // Open the shared memory segment
            shared_memory_object shm(open_only, m_shmName.c_str(), read_only);

            // Map the whole shared memory segment
            mapped_region region(shm, read_only);

            // Get the address of the mapped region
            void *raw_data = region.get_address();
            size_t region_size = region.get_size();

            // Validate the shared memory region
            if (!raw_data || region_size == 0)
            {
                std::cerr << "Invalid shared memory region" << std::endl;
                return;
            }

            // Ensure we have enough data for a 640x480x3 image
            size_t expected_size = 640 * 480 * 3;
            if (region_size < expected_size)
            {
                std::cerr << "Insufficient shared memory size" << std::endl;
                return;
            }

            // Create a copy of the data to ensure safe access
            std::vector<unsigned char> image_data(expected_size);
            std::memcpy(image_data.data(), raw_data, expected_size);

            // Create OpenCV Mat from copied data
            Mat frame(480, 640, CV_8UC3, image_data.data());

            // Validate the frame
            if (frame.empty() || frame.data == nullptr)
            {
                std::cerr << "Failed to create frame" << std::endl;
                return;
            }

            try
            {
                // Create a deep copy of the frame to ensure data integrity
                Mat safeCopy = frame.clone();

                cv::Mat rgbFrame;
                cv::cvtColor(safeCopy, rgbFrame, cv::COLOR_BGR2RGB);

                // Use rgbFrame instead of safeCopy
                QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
                imageLabel->setPixmap(QPixmap::fromImage(qimg));
            }
            catch (const cv::Exception &e)
            {
                std::cerr << "Color conversion error: " << e.what() << std::endl;
            }

        
            // // Convert BGR to RGB
            // // cv::cvtColor(safeCopy, safeCopy, cv::COLOR_BGR2RGB);

            // // Convert OpenCV Mat to QImage
            // QImage qimg(safeCopy.data, safeCopy.cols, safeCopy.rows, safeCopy.step, QImage::Format_RGB888);

            // // Update the label on the main thread
            // QMetaObject::invokeMethod(imageLabel, [this, qimg]() {
            //     imageLabel->setPixmap(QPixmap::fromImage(qimg));
            // }, Qt::QueuedConnection);
        }
        catch (const boost::interprocess::interprocess_exception &e)
        {
            std::cerr << "Boost Interprocess Error -> for shared mem name: " << m_shmName << " " << e.what() << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Standard Exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error occurred" << std::endl;
        }
    }

private:
    QLabel *imageLabel;
    QTimer *timer;
    std::string m_shmName;  // Store the shared memory name
};

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    // Default shared memory name if no argument provided
    std::string shm_name = "SharedFrame";

    // Check if a custom shared memory name is provided
    if (argc > 1) {
        shm_name += argv[1];
        cout << shm_name << endl;
    }
    
    SharedMemoryViewer viewer(shm_name);
    viewer.show();
    return app.exec();
}