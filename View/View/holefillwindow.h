#ifndef VIEW_HOLEFILLWINDOW_H
#define VIEW_HOLEFILLWINDOW_H

#include <View/holefilllabel.h>

#include <PatchMatch/holefillpatchmatch.h>
#include <PatchMatch/holefillpatchmatchopencl.h>

#include <View/opencldeviceselection.h>

#include <OpenCL/device.h>

#include <Core/image/imagetypes.h>

#include <QMainWindow>

#include <boost/optional.hpp>

#include <memory>
#include <vector>

namespace view {

namespace Ui {
class HoleFillWindow;
}

class HoleFillWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit HoleFillWindow(QWidget *parent = 0);
    ~HoleFillWindow();
protected:
    void closeEvent(QCloseEvent *);
private slots:
    void loadImageSlot();
    void pickOpenCLDeviceSlot();
    void startManualFillSlot();
    void automaticFillCPUSlot();
    void automaticFillOpenCLSlot();
    void refineNNFSlot();
    void nextPyramidSlot();
    void quitManualFillSlot();
private:
    void displayTimeTaken( double seconds);
    QString programName() { return "PatchMatch"; }
    QString companyName() { return "Greg Philbrick"; }
    /// Return whether the input image was successfully replaced with the indicated image file.
    bool loadTargetImage( const std::string& inputImagePath, bool loadHoleToo, bool showFailureDialogs );
    void loadTargetImage( core::ImageRGB&& );
    void readSettings();
    void writeSettings();
    /// Store to file which pixels of '_targetImage' the user has marked for hole filling.
    void recordHole();
    void attemptInitialOpenCLDeviceSetup();
    void respondToDeviceSelection( const boost::optional< openCL::Device >& );
    void updateNextPyramidLevelButton();

    QString saveHolePath() { return "holeUsedForPatchMatch"; }

    /// Info re. the last selected OpenCL device, can be saved between runs of exe.
    view::OpenCLDeviceSelection _lastDeviceSelection;
    std::vector< openCL::Device > _availableDevices;

    Ui::HoleFillWindow *ui;
    QString _lastInputImageFilePath;
    int _patchWidth;
    HoleFillLabel* _label;
    core::ImageRGB _targetImage;

    std::unique_ptr< patchMatch::HoleFillPatchMatch > _patchMatchCPU;
    std::unique_ptr< patchMatch::HoleFillPatchMatchOpenCL > _patchMatchOpenCL;

    // For when we do manual filling
    core::ImageBinary _backupHole;

    static const int _numPyramidLevels;
};

} // view

#endif // #include guard
