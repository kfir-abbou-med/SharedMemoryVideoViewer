#include <QWidget>
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QSlider>
#include <QPushButton>

#include "Message.h"
#include <opencv2/opencv.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <iostream>
#include <cstring>
#include <boost/asio.hpp>
#include <string>
#include <sstream>

using namespace boost::interprocess;
using namespace cv;
using namespace std;

const std::string serverIP = "127.0.0.1"; // Replace with your server's IP address
const int serverPort = 8080;              // Replace with your server's port

class SharedMemoryViewer : public QMainWindow
{
public:
    SharedMemoryViewer(const std::string &shm_name, QWidget *parent = nullptr) : QMainWindow(parent), m_shmName(shm_name)
    {
        imageLabel = new QLabel(this);

        // Create sliders for brightness and zoom
        QSlider *brightnessSlider = new QSlider(Qt::Horizontal, this);
        brightnessSlider->setRange(0, 200); // 0 to 200 for brightness factor adjustment
        brightnessSlider->setValue(100);    // Default value (normal brightness)

        QLabel *brightnessLabel = new QLabel("Brightness", this); // Label for brightness

        QSlider *zoomSlider = new QSlider(Qt::Horizontal, this);
        zoomSlider->setRange(100, 200); // 100% to 200% zoom
        zoomSlider->setValue(100);      // Default value (no zoom)

        QLabel *zoomLabel = new QLabel("Zoom", this); // Label for zoom

        // Create Start and Stop buttons
        QPushButton *startButton = new QPushButton("Start", this);
        QPushButton *stopButton = new QPushButton("Stop", this);

        // Set up layout
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // Add the image label
        mainLayout->addWidget(imageLabel);

        // Add sliders with their labels
        QHBoxLayout *brightnessLayout = new QHBoxLayout();
        brightnessLayout->addWidget(brightnessLabel);
        brightnessLayout->addWidget(brightnessSlider);

        QHBoxLayout *zoomLayout = new QHBoxLayout();
        zoomLayout->addWidget(zoomLabel);
        zoomLayout->addWidget(zoomSlider);

        // Add buttons to the layout
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addWidget(startButton);
        buttonLayout->addWidget(stopButton);

        mainLayout->addLayout(brightnessLayout);
        mainLayout->addLayout(zoomLayout);
        mainLayout->addLayout(buttonLayout);

        QWidget *centralWidget = new QWidget(this);
        centralWidget->setLayout(mainLayout);
        setCentralWidget(centralWidget);

        resize(800, 800);
        setWindowTitle(("Shared Memory Frame Viewer: " + shm_name).c_str());

        // Connect slider signals to respective slots
        connect(brightnessSlider, &QSlider::valueChanged, this, &SharedMemoryViewer::onBrightnessSliderValueChanged);
        connect(zoomSlider, &QSlider::valueChanged, this, &SharedMemoryViewer::onZoomSliderValueChanged);

        // Connect button signals to respective slots
        connect(startButton, &QPushButton::clicked, this, &SharedMemoryViewer::onStartButtonClicked);
        connect(stopButton, &QPushButton::clicked, this, &SharedMemoryViewer::onStopButtonClicked);

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

    // Function to send the message over TCP

    void sendMessage(const std::string &ip, int port, const json &message)
    {
        try
        {
            boost::asio::io_context ioContext;
            boost::asio::ip::tcp::socket socket(ioContext);

            boost::asio::ip::tcp::resolver resolver(ioContext);
            auto endpoints = resolver.resolve(ip, std::to_string(port));
            boost::asio::connect(socket, endpoints);

            // Convert the JSON object to a string and add a newline
            std::string messageStr = message.dump() + "\n";

            boost::asio::write(socket, boost::asio::buffer(messageStr));
            std::cout << "Message sent: " << message << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error sending message: " << e.what() << std::endl;
        }
    }

    void onBrightnessSliderValueChanged(int value)
    {
        cout << "onBrightnessSliderValueChanged " << value << ", m_shmName: " << m_shmName << endl;
        double valD = value;
        valD = valD / 100.0;

        // Create an UpdateSettings message
        ClientMessage updateSettingsMsg = ClientMessage::createUpdateSettingsMessage(m_shmName, "brightness", std::to_string(valD));

        // Serialize the message to JSON
        json serializedMessage = updateSettingsMsg.serialize();

        // Print the serialized message
        std::cout << "Serialized Message: " << serializedMessage.dump(4) << std::endl;

        // cout << "val2: " << valD << endl;
        // std::string valStr = std::to_string(valD);
        // Message message("BRIGHTNESS", valStr);
        // sendMessage("127.0.0.1", 8080, "{\"sourceId\": \"SharedFrame" + m_shmName + "\", \"propertyName\": \"brightness\", \"propertyValue\": " + valStr + " }");
        sendMessage("127.0.0.1", 8080, serializedMessage);
    }

    void onZoomSliderValueChanged(int value)
    {
        cout << "onZoomSliderValueChanged: " << value << endl;
        double valD = value / 100.0;
        std::string valStr = std::to_string(valD);
        // Message message("ZOOM", valStr);
        // sendMessage("127.0.0.1", 8080, "{\"sourceId\": \"SharedFrame" + m_shmName + "\", \"propertyName\": \"zoom\", \"propertyValue\": " + valStr + " }");

        // Create an UpdateSettings message
        ClientMessage updateSettingsMsg = ClientMessage::createUpdateSettingsMessage(m_shmName, "zoom", std::to_string(valD));

        // Serialize the message to JSON
        json serializedMessage = updateSettingsMsg.serialize();

        // Print the serialized message
        std::cout << "Serialized Message: " << serializedMessage.dump(4) << std::endl;

        sendMessage("127.0.0.1", 8080, serializedMessage);
    }

    void onStartButtonClicked()
    {
        std::cout << "Start button clicked!" << std::endl;
        ClientMessage startMsg = ClientMessage::createCommandMessage(m_shmName, "start");

          // Serialize the message to JSON
        json serializedMessage = startMsg.serialize();

        // Print the serialized message
        std::cout << "Serialized Message: " << serializedMessage.dump(4) << std::endl;

        sendMessage("127.0.0.1", 8080, serializedMessage);
    }

    void onStopButtonClicked()
    {
        std::cout << "Stop button clicked!" << std::endl;
        ClientMessage stopMsg = ClientMessage::createCommandMessage(m_shmName, "stop");

          // Serialize the message to JSON
        json serializedMessage = stopMsg.serialize();

        // Print the serialized message
        std::cout << "Serialized Message: " << serializedMessage.dump(4) << std::endl;

        sendMessage("127.0.0.1", 8080, serializedMessage);
    }

private:
    QLabel *imageLabel;
    QTimer *timer;
    std::string m_shmName; // Store the shared memory name
    boost::asio::io_context io_context;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    std::string shm_name = "SharedFrame";

    if (argc > 1)
    {
        shm_name += argv[1];
        cout << shm_name << endl;
    }

    SharedMemoryViewer viewer(shm_name);
    viewer.show();
    return app.exec();
}
