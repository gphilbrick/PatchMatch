#include <holefillwindow.h>

#include <ui_holefillwindow.h>

#include <View/selectopencldevicedialog.h>

#include <OpenCL/platform.h>

#include <CoreQt/qdatastreamutility.h>
#include <CoreQt/qtinterfaceutility.h>

#include <Core/exceptions/runtimeerror.h>
#include <Core/image/imageUtility.h>

#include <QElapsedTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

namespace view {

const char* const settingsKey_lastInputImageFilePath = "lastInputImageFilePath";
const int numPyramidLevels = 10;
const QDataStream::Version qDataStreamVersion = QDataStream::Qt_5_1;

constexpr int auto_numRounds = 4;
constexpr int auto_refinesPerRound = 3;
constexpr int auto_numRounds_baseLevel = 8;
constexpr int auto_refinesPerRound_baseLevel = 5;

HoleFillWindow::HoleFillWindow( QWidget *parent ) :
    QMainWindow( parent ),
    ui( new Ui::HoleFillWindow )
{
    _patchWidth = 7;

    ui->setupUi(this);

    _label = new HoleFillLabel();
    ui->scrollArea->setWidget(_label);

    connect(ui->actionOpenCLDevice, SIGNAL(triggered()), this, SLOT(pickOpenCLDeviceSlot()));
    connect(ui->actionLoad, SIGNAL(triggered()), this, SLOT(loadImageSlot()));
    connect(ui->brushWidthSpinBox,SIGNAL(valueChanged(int)),_label,SLOT(brushWidthSlot(int)));
    connect(ui->clearHoleButton,SIGNAL(clicked()),_label,SLOT(clearHoleSlot()));
    connect(ui->startManualButton,SIGNAL(clicked()),this,SLOT(startManualFillSlot()));
    connect(ui->refineNNFPushButton,SIGNAL(clicked()),this,SLOT(refineNNFSlot()));
    connect(ui->nextPyramidButton,SIGNAL(clicked()),this,SLOT(nextPyramidSlot()));
    connect(ui->quitManualButton,SIGNAL(clicked()),this,SLOT(quitManualFillSlot()));
    connect(ui->automaticFillButton,SIGNAL(clicked()),this,SLOT(automaticFillCPUSlot()));
    connect(ui->automaticFillOpenCLButton,SIGNAL(clicked()),this,SLOT(automaticFillOpenCLSlot()));

    readSettings();
    attemptInitialOpenCLDeviceSetup();

    // Setup target image.
    {
        bool loadedAnImage = false;
        if (!_lastInputImageFilePath.isEmpty()) {
            // Try to load the saved hole file at same time.
            loadedAnImage = loadTargetImage(_lastInputImageFilePath.toStdString(), true, false );
        }
        if ( !loadedAnImage) {
            // Fall back on the image shipped w/ the repository.
            loadedAnImage = loadTargetImage("runtimeResources/images/pexels-photo-5712934.jpeg", false, false );
        }
        if (!loadedAnImage) {
            // Generate a test checkerboard image.
            const int w = 640;
            const int h = 480;
            const int cellW = 30;
            core::ImageRGB checkerboard(w, h);
            checkerboard.set(
                [&](int x, int y) {
                    const bool bOrW = (x / cellW) % 2 == (y / cellW) % 2;
                    return bOrW
                        ? core::Vector3(0, 0, 0)
                        : core::Vector3(1, 1, 1);
                });
            loadTargetImage( std::move(checkerboard) );
        }
    }
}

HoleFillWindow::~HoleFillWindow()
{
    delete ui;
}

void HoleFillWindow::pickOpenCLDeviceSlot()
{
    if (_availableDevices.size()) {
        auto* const pickDeviceDialog = new view::SelectOpenCLDeviceDialog(_availableDevices, _lastDeviceSelection, this);
        pickDeviceDialog->exec();
        if (pickDeviceDialog->result() == QDialog::Accepted) {
            const auto res = pickDeviceDialog->device();
            if (res) {
                _lastDeviceSelection = view::OpenCLDeviceSelection( *res );
            }
            respondToDeviceSelection( res );
        }
    }
}

void HoleFillWindow::attemptInitialOpenCLDeviceSetup()
{
    boost::optional< openCL::Device > devToUse;
    _availableDevices.clear();
    const auto platforms = openCL::Platform::platforms();
    for (const auto& p : platforms) {
        for (const auto& d : p.devices()) {
            if (d.supportsImages()) {
                _availableDevices.push_back(d);
            }
        }
    }

    ui->actionOpenCLDevice->setEnabled( _availableDevices.size() > 0 );
    if (_availableDevices.size() == 0) {
        QMessageBox::warning(
            this,
            programName(),
            "No image-supporting OpenCL device found. You can still perform operations using the non-OpenCL implementation." );
    } else {
        // Do we remember a device selection from the last time we ran?
        for (const auto& d : _availableDevices) {
            if (_lastDeviceSelection.matches(d)) {
                devToUse = d;
                break;
            }
        }
        if ( !devToUse ) {
            // Either this is first ever run of exe or the user altered opencl installation[s] 
            // between runs.
            auto pickDialog = new view::SelectOpenCLDeviceDialog(_availableDevices, _lastDeviceSelection, this);
            pickDialog->exec();
            devToUse = pickDialog->device();
            if (devToUse) {
                _lastDeviceSelection = view::OpenCLDeviceSelection(*devToUse);
            }
        }
    }
    respondToDeviceSelection( devToUse );
}

void HoleFillWindow::respondToDeviceSelection( const boost::optional< openCL::Device >& dev )
{
    quitManualFillSlot();
    _patchMatchOpenCL = nullptr;
    if (dev) {
        _patchMatchOpenCL = std::make_unique< patchMatch::HoleFillPatchMatchOpenCL >(*dev);
    }
    ui->automaticFillOpenCLButton->setEnabled( _patchMatchOpenCL != nullptr );
}

void HoleFillWindow::loadTargetImage( core::ImageRGB&& toOwn )
{
    _targetImage = std::move( toOwn );
    _label->clearHole();
    _label->loadLayer(_targetImage, 0);
    if (ui->manualFillGroupBox->isEnabled())
    {
        quitManualFillSlot();
    }
}

bool HoleFillWindow::loadTargetImage(const std::string& inputImagePath, bool loadHoleFileAlso, bool showFailureDialogs )
{
    core::ImageRGB loadedImage;
    const auto loadResult = coreQt::loadRgbImageFromFile(
        loadedImage, 
        inputImagePath );
    if (loadResult)
    {
        // Forbid too-small images.
        if (loadedImage.width() < _patchWidth || loadedImage.height() < _patchWidth) {
            if (showFailureDialogs) {
                QMessageBox::warning(
                    this,
                    programName(),
                    QString::fromStdString("The image at " + inputImagePath + " is too small."));
            }
            return false;
        } 
        loadTargetImage( std::move(loadedImage ) );
        _lastInputImageFilePath = QString::fromStdString(inputImagePath);

        if ( loadHoleFileAlso ) {
            QFile inFile( saveHolePath() );
            if (inFile.open(QIODevice::ReadOnly))
            {
                core::ImageBinary readTo;
                QDataStream inStream(&inFile);
                inStream.setVersion( qDataStreamVersion );
                coreQt::readFromDataStream(inStream, readTo);

                if (inStream.status() == QDataStream::Ok)
                {
                    if (readTo.width() == _targetImage.width() &&
                        readTo.height() == _targetImage.height())
                    {
                        _label->toggleHoleVisibility(true);
                        _label->setHole(readTo);
                    }
                }
            }
        }
        return true;
    } else {
        if (showFailureDialogs) {
            QMessageBox::warning(
                this,
                programName(),
                QString::fromStdString("Failed to load image " + inputImagePath));
        }
        return false;
    }
}

void HoleFillWindow::refineNNFSlot()
{
    const int numIterations = ui->numRefinesSpinBox->value();
    for(int i = 0; i < numIterations; i++ )
    {
        _patchMatchCPU->search();
        _patchMatchCPU->propagate();
    }
    core::ImageRGB target;
    _patchMatchCPU->blend();
    _patchMatchCPU->getTargetImagePyramidSize( target );
    _label->loadLayer( target, 0, true );
}

void HoleFillWindow::updateNextPyramidLevelButton()
{
    const auto currentPyramidLevel = _patchMatchCPU->currentPyramidLevel();
    if ( currentPyramidLevel == 0 ) {
        ui->nextPyramidButton->setEnabled( false );
        ui->nextPyramidButton->setText( "Go to pyramid level 0" );
    } else {
        ui->nextPyramidButton->setEnabled( true );
        ui->nextPyramidButton->setText( "Go to pyramid level " + QString::number( currentPyramidLevel - 1 ) );
    }
}

void HoleFillWindow::nextPyramidSlot()
{
    _patchMatchCPU->moveToNextPyramidLevel();
    updateNextPyramidLevelButton();
    core::ImageRGB newTarget;
    _patchMatchCPU->getTargetImagePyramidSize(newTarget);
    _label->loadLayer( newTarget, 0 );
}

void HoleFillWindow::recordHole()
{
    QFile outFile(saveHolePath());
    if(!outFile.open(QIODevice::WriteOnly)) {
        return;
    }
    QDataStream outStream(&outFile);
    outStream.setVersion( qDataStreamVersion ); 
    coreQt::writeToDataStream( outStream, _label->hole() );
    outFile.close();
}

void HoleFillWindow::displayTimeTaken(double seconds)
{
    const auto megapixels = ((double)(_targetImage.width() * _targetImage.height())) * 0.000001;

    QString message = "Took "
        + QString::number(seconds) + " seconds to run on an image with "
        + QString::number(megapixels)
        + " megapixels.";
    QMessageBox::warning(this, "Timing", message);
}

void HoleFillWindow::automaticFillOpenCLSlot()
{
    if (!_patchMatchOpenCL) {
        THROW_RUNTIME("Should not reach");
    }

    recordHole();

	QElapsedTimer stopwatch;
    stopwatch.start();

    _patchMatchOpenCL->init(
        _targetImage,
        _label->hole(),
        numPyramidLevels,
        _patchWidth);

    // Do base pyramid level.
    for( int i = 0; i < auto_numRounds_baseLevel; i++ ) {
        for( int i = 0; i < auto_refinesPerRound_baseLevel; i++ ) {
            _patchMatchOpenCL->planStep( patchMatch::HoleFillPatchMatchOpenCL::Search);
            _patchMatchOpenCL->planStep( patchMatch::HoleFillPatchMatchOpenCL::Propagate);
        }
        _patchMatchOpenCL->planStep(patchMatch::HoleFillPatchMatchOpenCL::Blend);
    }

    // Do the rest of the pyramid levels
    for( int i = 0; i < numPyramidLevels-1; i++ ) {
        _patchMatchOpenCL->planStep(patchMatch::HoleFillPatchMatchOpenCL::NextPyramid);
        for( int j = 0; j < auto_numRounds; j++ ) {
            for( int k = 0; k < auto_refinesPerRound; k++ ) {
                _patchMatchOpenCL->planStep(patchMatch::HoleFillPatchMatchOpenCL::Search);
                _patchMatchOpenCL->planStep(patchMatch::HoleFillPatchMatchOpenCL::Propagate);
            }
            _patchMatchOpenCL->planStep(patchMatch::HoleFillPatchMatchOpenCL::Blend);
        }
    }

    //display the final results
    core::ImageRGB display;
    _patchMatchOpenCL->executeSteps(display);
    const auto timeElapsed = (double)stopwatch.elapsed() / 1000.;
    displayTimeTaken(timeElapsed);

    _label->brieflyHideHole();
    _label->loadLayer(display,0,true);
}

void HoleFillWindow::automaticFillCPUSlot()
{
    recordHole();

    QElapsedTimer stopwatch;
    stopwatch.start();

    _patchMatchCPU = std::make_unique< patchMatch::HoleFillPatchMatch >(
        _patchWidth,
        _targetImage,
        _targetImage,
        _label->hole(),
        numPyramidLevels );

    // Do base level.
    for (int i = 0; i < auto_numRounds_baseLevel; i++) {
        for (int i = 0; i < auto_refinesPerRound_baseLevel; i++) {
            _patchMatchCPU->search();
            _patchMatchCPU->propagate();
        }
        _patchMatchCPU->blend();
    }
    // Later pyramid levels.
    while ( _patchMatchCPU->currentPyramidLevel() > 0) {
        _patchMatchCPU->moveToNextPyramidLevel();
        for( int round = 0; round < auto_numRounds; round++ ) {
            for( int refine = 0; refine < auto_refinesPerRound; refine++ ) {
                _patchMatchCPU->search();
                _patchMatchCPU->propagate();
            }
            _patchMatchCPU->blend();
        }
    } 

    const auto timeElapsed = (double)stopwatch.elapsed() / 1000.;
    displayTimeTaken( timeElapsed );

    // Display the final results
    _label->brieflyHideHole();
    core::ImageRGB display;
    _patchMatchCPU->getTargetImagePyramidSize( display );
    _label->loadLayer( display, 0 );
}

void HoleFillWindow::quitManualFillSlot()
{
    if (_backupHole.size() == _targetImage.size()) {
        _label->setHole(_backupHole);
    }
    _label->toggleHoleVisibility(true);
    _label->loadLayer( _targetImage, 0 );
    ui->manualFillGroupBox->setEnabled(false);
    ui->clearHoleButton->setEnabled(true);
    ui->brushWidthSpinBox->setEnabled(true);
    ui->automaticFillButton->setEnabled(true);
    ui->automaticFillOpenCLButton->setEnabled(true);
}

void HoleFillWindow::startManualFillSlot()
{
    core::ImageBinary::clone(_label->hole(), _backupHole);
    recordHole();

    _patchMatchCPU = std::make_unique< patchMatch::HoleFillPatchMatch >(
        _patchWidth,
        _targetImage,
        _targetImage,
        _label->hole(),
        numPyramidLevels );

    // Show the initial target.
    core::ImageRGB display;
    _patchMatchCPU->getTargetImagePyramidSize( display );
    _label->loadLayer(display,0,true);

    _label->toggleHoleVisibility(false);
    ui->clearHoleButton->setEnabled(false);
    ui->brushWidthSpinBox->setEnabled(false);
    ui->automaticFillButton->setEnabled(false);
    ui->automaticFillOpenCLButton->setEnabled(false);
    ui->manualFillGroupBox->setEnabled(true);

    updateNextPyramidLevelButton();
}

void HoleFillWindow::closeEvent(QCloseEvent *)
{
    writeSettings();
}

void HoleFillWindow::loadImageSlot()
{
    QString filePath = QFileDialog::getOpenFileName(this,programName(),_lastInputImageFilePath,"Image file (*.jpg *.bmp *.png)");
    if(filePath.isEmpty()) return;
    loadTargetImage(filePath.toStdString(), false, true );
}

void HoleFillWindow::writeSettings()
{
    QSettings settings(companyName(),programName());
    settings.setValue(
        settingsKey_lastInputImageFilePath, _lastInputImageFilePath );
    _lastDeviceSelection.save(settings);
}

void HoleFillWindow::readSettings()
{
    QSettings settings(companyName(),programName());
    _lastInputImageFilePath = settings.value( settingsKey_lastInputImageFilePath ).toString();
    _lastDeviceSelection = view::OpenCLDeviceSelection(settings);
}

} // view

