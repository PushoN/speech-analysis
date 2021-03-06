//
// Created by rika on 06/12/2019.
//

#include "MainWindow.h"
#include "ColorMaps.h"
#include "FFT/FFT.h"
#include "../log/simpleQtLogger.h"
#ifdef Q_OS_ANDROID
    #include <QtAndroid>
    #include <QAndroidIntent>
    #include <jni.h>
    #include "../jni/JniInstance.h"
#endif

constexpr int maxWidthComboBox = 200;

MainWindow::MainWindow()
    : fftSizes{"64", "128", "256", "512", "1024", "2048"}
{

    L_INFO("Initialising miniaudio context...");

    std::vector<ma_backend> backends{
        ma_backend_wasapi,
        ma_backend_winmm,
        ma_backend_dsound,

        ma_backend_coreaudio,

        ma_backend_pulseaudio,
        ma_backend_jack,
        ma_backend_alsa,
    
        ma_backend_sndio,
        ma_backend_audio4,
        ma_backend_oss,

        ma_backend_aaudio,
        ma_backend_opensl,

        ma_backend_webaudio,

        ma_backend_null
    };

    ma_context_config ctxCfg = ma_context_config_init();
    ctxCfg.threadPriority = ma_thread_priority_realtime;
    ctxCfg.alsa.useVerboseDeviceEnumeration = false;
    ctxCfg.pulse.tryAutoSpawn = true;
    ctxCfg.jack.tryStartServer = true;

    if (ma_context_init(backends.data(), backends.size(), &ctxCfg, &maCtx) != MA_SUCCESS) {
        L_FATAL("Failed to initialise miniaudio context");

        throw AudioException("Failed to initialise miniaudio context");
    }
    
    devs = new AudioDevices(&maCtx);

    sineWave = new SineWave();

#ifdef Q_OS_MAC
    audioInterfaceMem = malloc(sizeof(AudioInterface));
    audioInterface = new (audioInterfaceMem) AudioInterface(&maCtx, sineWave);
#else
    audioInterface = new AudioInterface(&maCtx, sineWave);
#endif

    analyser = new Analyser(audioInterface);

    QPalette palette = this->palette();

    central = new QWidget;
    setCentralWidget(central);

    canvas = new AnalyserCanvas(analyser, sineWave);
    powerSpectrum = new PowerSpectrum(analyser, canvas);

#ifdef Q_OS_ANDROID
    JniInstance::createInstance(analyser, canvas, powerSpectrum);
#endif 

#ifdef Q_OS_ANDROID
    auto fieldsWidget = new QWidget(central);
    {
        auto ly1 = new QHBoxLayout(fieldsWidget);
#else
    fieldsDock = new QDockWidget("Estimates", this);
    {
        fieldsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        fieldsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

        auto dockWidget = new QWidget(fieldsDock);
        fieldsDock->setWidget(dockWidget);

        auto ly1 = new QBoxLayout(QBoxLayout::LeftToRight, dockWidget);

        connect(fieldsDock, &QDockWidget::dockLocationChanged,
                [&, ly1](Qt::DockWidgetArea area) {
                    if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea) {
                        ly1->setDirection(QBoxLayout::TopToBottom);
                    }
                    else if (area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea) {
                        ly1->setDirection(QBoxLayout::LeftToRight);
                    }
                });

        connect(fieldsDock, &QDockWidget::topLevelChanged,
                [&, ly1](bool topLevel) {
                    if (topLevel) {
                        ly1->setDirection(QBoxLayout::LeftToRight);
                    }
                });
#endif // Q_OS_ANDROID
        
        fieldOq = new QLineEdit;
        fieldOq->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        fieldOq->setReadOnly(true);

        ly1->addWidget(fieldOq, 0, Qt::AlignCenter);
        
        for (int i = 0; i < numFormants; ++i) {
            auto field = new QLineEdit;

            field->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
            field->setReadOnly(true);

            fieldFormant.push_back(field);

            ly1->addWidget(field, 0, Qt::AlignCenter);
        }

        fieldPitch = new QLineEdit;
        fieldPitch->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        fieldPitch->setReadOnly(true);

        ly1->addWidget(fieldPitch, 0, Qt::AlignCenter);

#ifdef Q_OS_ANDROID
        constexpr int buttonSize = 96;

        inputSettings = new QPushButton;
        inputSettings->setFixedSize(buttonSize, buttonSize);
        inputSettings->setStyleSheet("QPushButton { border-image: url(:/icons/settings.png) 0 0 0 0 stretch stretch; border: none; }");

        connect(inputSettings, &QPushButton::clicked,
                [&]() { openSettings(); });

        ly1->addSpacing(16);
        ly1->addWidget(inputSettings, 0, Qt::AlignRight);
#endif

    }

#ifndef Q_OS_ANDROID
    settingsDock = new QDockWidget("Settings", this);
    {
        settingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        settingsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

        auto dockScroll = new QScrollArea;
        auto dockWidget = new QWidget(settingsDock);
        settingsDock->setWidget(dockScroll);
        
        auto ly1 = new QFormLayout(dockWidget);
        ly1->setSizeConstraint(QLayout::SetNoConstraint);
        {
            auto devWidget = new QWidget;
            auto devLayout = new QHBoxLayout(devWidget);
            devLayout->setSizeConstraint(QLayout::SetMaximumSize);
            devLayout->setContentsMargins(0, 0, 0, 0);
            {
                inputDevIn = new QComboBox;
                inputDevIn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
                inputDevIn->setMaximumWidth(150);

                inputDevRefresh = new QPushButton;
                inputDevRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
                inputDevRefresh->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

                connect(inputDevRefresh, &QPushButton::clicked,
                        [&]() { updateDevices(); });

                devLayout->addWidget(inputDevIn, 0, Qt::AlignLeft);
                devLayout->addWidget(inputDevRefresh, 0, Qt::AlignRight);
            }

            inputDisplayDialog = new QPushButton("Open dialog");
        
            connect(inputDisplayDialog, &QPushButton::clicked,
                    [&]() {
                        dialogDisplay->setVisible(true);
                        dialogDisplay->activateWindow();
                        dialogDisplay->setFocus(Qt::ActiveWindowFocusReason);
                    });

            inputFftSize = new QComboBox;
            inputFftSize->addItems(fftSizes);

            connect(inputFftSize, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
                    [&](const QString value) { analyser->setFftSize(value.toInt()); });

            inputLpOrder = new QSpinBox;
            inputLpOrder->setRange(5, 22);

            connect(inputLpOrder, QOverload<int>::of(&QSpinBox::valueChanged),
                    [&](const int value) { analyser->setLinearPredictionOrder(value); });

            inputMaxFreq = new QSpinBox;
            inputMaxFreq->setRange(2500, 7000);
            inputMaxFreq->setStepType(QSpinBox::AdaptiveDecimalStepType);
            inputMaxFreq->setSuffix(" Hz");

            connect(inputMaxFreq, QOverload<int>::of(&QSpinBox::valueChanged),
                    [&](const int value) { analyser->setMaximumFrequency(value); });

            inputFrameLength = new QSpinBox;
            inputFrameLength->setRange(25, 80);
            inputFrameLength->setSingleStep(5);
            inputFrameLength->setSuffix(" ms");

            connect(inputFrameLength, QOverload<int>::of(&QSpinBox::valueChanged),
                    [&](const int value) { analyser->setFrameLength(std::chrono::milliseconds(value)); });

            inputFrameSpace = new QSpinBox;
            inputFrameSpace->setRange(1, 30);
            inputFrameSpace->setSingleStep(1);
            inputFrameSpace->setSuffix(" ms");

            connect(inputFrameSpace, QOverload<int>::of(&QSpinBox::valueChanged),
                    [&](const int value) { analyser->setFrameSpace(std::chrono::milliseconds(value)); });

            inputWindowSpan = new QDoubleSpinBox;
            inputWindowSpan->setRange(2, 30);
            inputWindowSpan->setSingleStep(0.5);
            inputWindowSpan->setSuffix(" s");

            connect(inputWindowSpan, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    [&](const double value) { analyser->setWindowSpan(std::chrono::milliseconds(int(1000 * value))); });

            inputPitchAlg = new QComboBox;
            inputPitchAlg->addItems({
                "Wavelet",
                "McLeod",
                "YIN",
                "AMDF",
            });

            connect(inputPitchAlg, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [&](const int value) { analyser->setPitchAlgorithm(static_cast<PitchAlg>(value)); });

            inputFormantAlg = new QComboBox;
            inputFormantAlg->addItems({
                "Linear prediction",
                "Kalman filter",
            });

            connect(inputFormantAlg, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [&](const int value) { analyser->setFormantMethod(static_cast<FormantMethod>(value)); });

            ly1->addRow("Audio device:", devWidget);
            ly1->addRow("Display settings:", inputDisplayDialog);
            ly1->addRow("FFT size:", inputFftSize);
            ly1->addRow("Linear prediction order:", inputLpOrder);
            ly1->addRow("Maximum frequency:", inputMaxFreq);
            ly1->addRow("Frame length:", inputFrameLength);
            ly1->addRow("Frame space:", inputFrameSpace);
            ly1->addRow("Analysis duration:", inputWindowSpan);

            ly1->addRow("Pitch algorithm:", inputPitchAlg);
            ly1->addRow("Formant algorithm:", inputFormantAlg);
        }
        
        dockScroll->setWidget(dockWidget);
    }
   
    dialogDisplay = new QWidget(window(), Qt::Tool);
    {
        auto ly1 = new QFormLayout(dialogDisplay);
        
        inputToggleSpectrum = new QCheckBox;

        connect(inputToggleSpectrum, &QCheckBox::toggled,
                [&](const bool checked) { canvas->setDrawSpectrum(checked); });

        inputToggleTracks = new QCheckBox;

        connect(inputToggleTracks, &QCheckBox::toggled,
                [&](const bool checked) { canvas->setDrawTracks(checked); });

        inputMinGain = new QSpinBox;
        inputMinGain->setRange(-200, 60);
        inputMinGain->setSingleStep(10);
        inputMinGain->setSuffix(" dB");

        connect(inputMinGain, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](const int value) { canvas->setMinGainSpectrum(value);
                                       inputMaxGain->setMinimum(value + 10); });

        inputMaxGain = new QSpinBox;
        inputMaxGain->setRange(-200, 60);
        inputMaxGain->setSingleStep(10);
        inputMaxGain->setSuffix(" dB");

        connect(inputMaxGain, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](const int value) { canvas->setMaxGainSpectrum(value);
                                       inputMinGain->setMaximum(value - 10); });

        inputFreqScale = new QComboBox;
        inputFreqScale->addItems({"Linear", "Logarithmic", "Mel"});

         connect(inputFreqScale, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [&](const int value) { canvas->setFrequencyScale(value); });

        inputPitchThick = new QSpinBox;
        inputPitchThick->setRange(1, 20);
        inputPitchThick->setSingleStep(1);
        inputPitchThick->setSuffix(" px");
        
        connect(inputPitchThick, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](const int value) { canvas->setPitchThickness(value); });

        inputPitchColor = new QPushButton;

        connect(inputPitchColor, &QPushButton::clicked,
                [=] () {
                    QColor c = QColorDialog::getColor(
                        canvas->getPitchColor(),
                        this,
                        QString("Select pitch color"),
                        QColorDialog::DontUseNativeDialog
                    );
                    if (c.isValid()) {
                        canvas->setPitchColor(c);
                        updateColorButtons();
                    }
                });

        inputFormantThick = new QSpinBox;
        inputFormantThick->setRange(1, 20);
        inputFormantThick->setSingleStep(1);
        inputFormantThick->setSuffix(" px");
        
        connect(inputFormantThick, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](const int value) { canvas->setFormantThickness(value); });

        for (int nb = 0; nb < numFormants; ++nb) {
            auto input = new QPushButton;
            
            connect(input, &QPushButton::clicked,
                    [=] () {
                        QColor c = QColorDialog::getColor(
                            canvas->getFormantColor(nb),
                            this,
                            QString("Select F%1 color").arg(nb + 1),
                            QColorDialog::DontUseNativeDialog
                        );
                        if (c.isValid()) {
                            canvas->setFormantColor(nb, c);
                            updateColorButtons();
                        }
                    });

            inputFormantColor[nb] = input;
        }

        inputColorMap = new QComboBox;
        for (auto & [name, map] : colorMaps) {
            inputColorMap->addItem(name);
        }

        connect(inputColorMap, QOverload<const QString &>::of(&QComboBox::currentIndexChanged),
                [&](const QString & name) { canvas->setSpectrumColor(name); });

        ly1->addRow("Show spectrum:", inputToggleSpectrum);
        ly1->addRow("Show tracks:", inputToggleTracks);
        ly1->addRow("Minimum gain:", inputMinGain);
        ly1->addRow("Maximum gain:", inputMaxGain);
        ly1->addRow("Frequency scale:", inputFreqScale);

        ly1->addRow("Pitch thickness:", inputPitchThick);
        ly1->addRow("Pitch color:", inputPitchColor);

        ly1->addRow("Formant thickness:", inputFormantThick);
        for (int nb = 0; nb < numFormants; ++nb) {
            const QString labelStr = QString("F%1 color:").arg(nb + 1);
            ly1->addRow(qPrintable(labelStr), inputFormantColor[nb]);
        }

        ly1->addRow("Spectrum color map:", inputColorMap);
    }
#endif // Q_OS_ANDROID

    auto lyCentral = new QVBoxLayout(central);
    {
        auto ly2 = new QHBoxLayout;
        lyCentral->addLayout(ly2);
        {
#ifdef Q_OS_ANDROID
            auto w1 = new QWidget;
            w1->setContentsMargins(0, 0, 0, 0);
            auto ly3 = new QHBoxLayout(w1);
            {
                ly3->addWidget(fieldsWidget);
            }
            ly2->addWidget(w1, 0, Qt::AlignCenter);
#else
            auto w2 = new QWidget;
            w2->setContentsMargins(0, 0, 0, 0);
            auto ly4 = new QHBoxLayout(w2);
            {
                constexpr int buttonSize = 32;

                auto github = new QPushButton;
                github->setFixedSize(buttonSize, buttonSize);
                github->setStyleSheet("QPushButton { border-image: url(:/icons/github.png) 0 0 0 0 stretch stretch; border: none; }");
                github->setCursor(Qt::PointingHandCursor);

                connect(github, &QPushButton::clicked, [&]() {
                            QDesktopServices::openUrl(QUrl("https://www.github.com/ichi-rika/speech-analysis"));
                        });

                auto patreon = new QPushButton;
                patreon->setFixedSize(buttonSize, buttonSize);
                patreon->setStyleSheet("QPushButton { border-image: url(:/icons/patreon.png) 0 0 0 0 stretch stretch; border: none; }");
                patreon->setCursor(Qt::PointingHandCursor);

                connect(patreon, &QPushButton::clicked, [&]() {
                            QDesktopServices::openUrl(QUrl("https://www.patreon.com/cloyunhee"));
                        });

                inputPause = new QPushButton;
                inputPause->setFixedSize(buttonSize, buttonSize);
                inputPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
               
                connect(inputPause, &QPushButton::clicked, [&]() { toggleAnalyser(); });

                inputFullscreen = new QPushButton("F");
                inputFullscreen->setFixedSize(buttonSize, buttonSize);
                inputFullscreen->setStyleSheet("QPushButton { padding: 0; font-weight: bold; }");
                inputFullscreen->setCheckable(true);

                connect(inputFullscreen, &QPushButton::clicked, [&]() { toggleFullscreen(); });

                ly4->addWidget(github);
                ly4->addSpacing(8);
                ly4->addWidget(patreon);
                ly4->addSpacing(8);
                ly4->addWidget(inputPause);
                ly4->addWidget(inputFullscreen);
            }

            ly2->addWidget(w2, 0, Qt::AlignRight);
#endif
        }

        auto ly3 = new QHBoxLayout;
        lyCentral->addLayout(ly3);
        { 
            ly3->addWidget(canvas, 3);
            canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            ly3->addWidget(powerSpectrum, 1);
            powerSpectrum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
    }

#ifndef Q_OS_ANDROID
    addDockWidget(Qt::TopDockWidgetArea, fieldsDock);
    addDockWidget(Qt::LeftDockWidgetArea, settingsDock);
#endif

    setWindowTitle(WINDOW_TITLE);
    resize(WINDOW_WIDTH, WINDOW_HEIGHT);

    window()->installEventFilter(this);
    dialogDisplay->installEventFilter(this);

    loadSettings();

#ifndef Q_OS_ANDROID
    updateDevices();
#endif
    analyser->startThread();

    connect(this, &MainWindow::newFramesTracks, canvas, &AnalyserCanvas::renderTracks);
    connect(this, &MainWindow::newFramesSpectrum, canvas, &AnalyserCanvas::renderSpectrogram);
    connect(this, &MainWindow::newFramesSpectrum, powerSpectrum, &PowerSpectrum::renderSpectrum);
    connect(this, &MainWindow::newFramesLpc, powerSpectrum, &PowerSpectrum::renderLpc);
    connect(this, &MainWindow::newFramesUI, canvas, &AnalyserCanvas::renderScaleAndCursor);

    connect(&timer, &QTimer::timeout, [&]() {
        analyser->callIfNewFrames(
                    0,
                    [this](auto&&... ts) { emit newFramesTracks(std::forward<decltype(ts)>(ts)...); },
                    [this](auto&&... ts) { emit newFramesSpectrum(std::forward<decltype(ts)>(ts)...); },
                    [this](auto&&... ts) { emit newFramesLpc(std::forward<decltype(ts)>(ts)...); },
                    [this](auto&&... ts) { emit newFramesUI(std::forward<decltype(ts)>(ts)...); }
        );
        updateFields();
        canvas->repaint();
        powerSpectrum->repaint();
    });
    timer.setTimerType(Qt::PreciseTimer);
#ifdef Q_OS_ANDROID
    timer.start(1000.0 / 60.0);
#else
    timer.start(1000.0 / 60.0);
#endif
    
    show();

}

MainWindow::~MainWindow() {
    delete analyser;

#ifdef Q_OS_MAC
    audioInterface->~AudioInterface();
    free(audioInterfaceMem);
#else
    delete audioInterface;
#endif

    delete sineWave;

    delete devs;
    ma_context_uninit(&maCtx);

    all_fft_cleanup();
}

void MainWindow::updateFields() {

    const int frame = canvas->getSelectedFrame();

    const auto & formants = analyser->getFormantFrame(frame);
    const double pitch = analyser->getPitchFrame(frame);
    const double Oq = analyser->getOqFrame(frame);

    QPalette palette = this->palette();

    fieldOq->setText(QString("Oq = %1").arg(Oq));

    for (int i = 0; i < numFormants; ++i) {
        if (i < formants.nFormants) {
            fieldFormant[i]->setText(QString("F%1 = %2 Hz").arg(i + 1).arg(round(formants.formant[i].frequency)));
        }
        else {
            fieldFormant[i]->setText("");
        }
    }

    if (pitch > 0) {
        fieldPitch->setText(QString("H1 = %1 Hz").arg(pitch, 0, 'f', 1));
    } else {
        fieldPitch->setText("Unvoiced");
    }
    fieldPitch->adjustSize();

}

#ifndef Q_OS_ANDROID

void MainWindow::updateDevices()
{
    const auto & inputs = devs->getInputs();
    const auto & outputs = devs->getOutputs();

    inputDevIn->disconnect();
    inputDevIn->clear();

    inputDevIn->addItem("Default input device", QVariant::fromValue(std::make_pair(true, (const ma_device_id *) nullptr)));

    for (const auto & dev : inputs) {
        const QString name = QString::fromLocal8Bit(dev.name.c_str());
        inputDevIn->addItem(name, QVariant::fromValue(std::make_pair(true, &dev.id)));
    }
   
    if (ma_context_is_loopback_supported(&maCtx)) {
        for (const auto & dev : outputs) { 
            const QString name = QString::fromLocal8Bit(dev.name.c_str());
            inputDevIn->addItem("(Loopback) " + name, QVariant::fromValue(std::make_pair(false, &dev.id)));
        }
    }

    inputDevIn->setCurrentIndex(0);

    connect(inputDevIn, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&](const int index) {
                if (index >= 0) {
                    const auto [ isInput, id ] = inputDevIn->itemData(index).value<DevicePair>();

                    if (isInput || id == nullptr) {
                        analyser->setInputDevice(id);
                    }
                    else {
                        analyser->setOutputDevice(id);
                    }
                }
            });
}

void MainWindow::updateColorButtons() {
    static QString css = QStringLiteral(" \
                            QPushButton \
                            { \
                                background-color: %1; \
                                border: 1px solid #32414B; \
                                border-radius: 4px; \
                                padding: 5px; \
                                outline: none; \
                                min-width: 80px; \
                            } \
                            QPushButton:hover \
                            { \
                                border: 1px solid #148CD2; \
                            } \
                        ");

    inputPitchColor->setStyleSheet(css.arg(canvas->getPitchColor().name()));
    
    for (int i = 0; i < numFormants; ++i) {
        inputFormantColor[i]->setStyleSheet(css.arg(canvas->getFormantColor(i).name()));
    }

}

void MainWindow::toggleAnalyser() {
    bool running = analyser->isAnalysing();

    analyser->toggle(!running);

    if (running) {
        inputPause->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        inputPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void MainWindow::toggleFullscreen() {
    bool isFullscreen = (windowState() == Qt::WindowFullScreen);

    if (isFullscreen) {
        inputFullscreen->setChecked(false);
        setWindowState(Qt::WindowNoState);
    }
    else {
        inputFullscreen->setChecked(true);
        setWindowState(Qt::WindowFullScreen);
    }
}

bool MainWindow::eventFilter(QObject * obj, QEvent * event)
{
    if (event->type() == QEvent::KeyPress) {
        const auto keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();

        if (obj == window()) {
            if (key == Qt::Key_Escape) {
                close();
                return true;
            }
            else if (key == Qt::Key_P) {
                toggleAnalyser();
                return true;
            }
            else if (key == Qt::Key_F) {
                toggleFullscreen();
                return true;
            }
        }
        else if (obj == dialogDisplay) {
            if (key == Qt::Key_Escape) {
                dialogDisplay->setVisible(false);
                window()->activateWindow();
                window()->setFocus(Qt::ActiveWindowFocusReason);
                return true;
            }
        }
    }
        
    return QObject::eventFilter(obj, event);
}

#else

void MainWindow::openSettings()
{
    QAndroidIntent intent(QtAndroid::androidActivity().object(), "fr.cloyunhee.speechanalysis.SettingsActivity");
    QtAndroid::startActivity(intent, 0);
}
#endif // Q_OS_ANDROID

void MainWindow::loadSettings()
{
    // Assume that analysis settings have already loaded and corrected if necessary.
   
    int nfft = analyser->getFftSize();
    int lpOrder = analyser->getLinearPredictionOrder();
    double maxFreq = analyser->getMaximumFrequency();
    int cepOrder = analyser->getCepstralOrder();
    double frameLength = analyser->getFrameLength().count();
    double frameSpace = analyser->getFrameSpace().count();
    double windowSpan = analyser->getWindowSpan().count();
    PitchAlg pitchAlg = analyser->getPitchAlgorithm();
    FormantMethod formantAlg = analyser->getFormantMethod();

    // Assume that canvas settings have already loaded and corrected if necessary.
   
    int freqScale = canvas->getFrequencyScale();
    bool drawSpectrum = canvas->getDrawSpectrum();
    bool drawTracks = canvas->getDrawTracks();
    int minGain = canvas->getMinGainSpectrum();
    int maxGain = canvas->getMaxGainSpectrum();
    int pitchThick = canvas->getPitchThickness();
    int formantThick = canvas->getFormantThickness();
    QString colorMapName = canvas->getSpectrumColor();

    // Find the combobox index for nfft.
    int fftInd = fftSizes.indexOf(QString::number(nfft));
    if (fftInd < 0) {
        fftInd = fftSizes.indexOf("512");
    }

#ifndef Q_OS_ANDROID

#define callWithBlocker(obj, call) do { QSignalBlocker blocker(obj); (obj) -> call; } while (false)

    callWithBlocker(inputFftSize, setCurrentIndex(fftInd));
    callWithBlocker(inputLpOrder, setValue(lpOrder));
    callWithBlocker(inputMaxFreq, setValue(maxFreq));
    callWithBlocker(inputFrameLength, setValue(frameLength));
    callWithBlocker(inputFrameSpace, setValue(frameSpace));
    callWithBlocker(inputWindowSpan, setValue(windowSpan));
    callWithBlocker(inputPitchAlg, setCurrentIndex(static_cast<int>(pitchAlg)));
    callWithBlocker(inputFormantAlg, setCurrentIndex(static_cast<int>(formantAlg)));

    callWithBlocker(inputToggleSpectrum, setChecked(drawSpectrum));
    callWithBlocker(inputToggleTracks, setChecked(drawTracks));
    callWithBlocker(inputFreqScale, setCurrentIndex(freqScale));
    callWithBlocker(inputMinGain, setValue(minGain));
    callWithBlocker(inputMaxGain, setValue(maxGain));
    callWithBlocker(inputPitchThick, setValue(pitchThick));
    callWithBlocker(inputFormantThick, setValue(formantThick));

    updateColorButtons();

    callWithBlocker(inputColorMap, setCurrentText(colorMapName));

#endif

    void updateFields();
}
