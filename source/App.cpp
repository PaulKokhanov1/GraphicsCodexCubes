/** \file App.cpp */
#include "App.h"

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
    initGLG3D(G3DSpecification());

    GApp::Settings settings(argc, argv);

    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.caption             = argv[0];

    // Set enable to catch more OpenGL errors
    // settings.window.debugContext     = true;
   
    settings.window.fullScreen          = false;
    if (settings.window.fullScreen) {
        settings.window.width           = 1920;
        settings.window.height          = 1080;
    } else {
        settings.window.height          = int(OSWindow::primaryDisplayWindowSize().y * 0.95f); 
        // Constrain ultra widescreen aspect ratios
        settings.window.width           = min(settings.window.height * 1920 / 1080, int(OSWindow::primaryDisplayWindowSize().x * 0.95f));

        // Make even
        settings.window.width  -= settings.window.width & 1;
        settings.window.height -= settings.window.height & 1;
    }
    settings.window.resizable           = ! settings.window.fullScreen;
    settings.window.framed              = ! settings.window.fullScreen;
    settings.window.defaultIconFilename = "icon.png";

    // Set to true for a significant performance boost if your app can't
    // render at the display frequency, or if you *want* to render faster
    // than the display.
    settings.window.asynchronous        = true;

    // Render slightly larger than the screen so that screen-space refraction, bloom,
    // screen-space AO, and screen-space reflection to look good at screen edges. Set to zero for
    // maximum performance and free memory. Increase the second argument to improve AO without affecting
    // color. The third argument is the resolution. Set to 0.5f to render at half-res and upscale,
    // 2.0f to supersample.
    settings.hdrFramebuffer.setGuardBandsAndSampleRate(64, 0, 1.0f);

    settings.renderer.deferredShading = true;
    settings.renderer.orderIndependentTransparency = true;

    settings.dataDir                       = FileSystem::currentDirectory();

    settings.screenCapture.outputDirectory = FilePath::concat(FileSystem::currentDirectory(), "../journal");
    if (! FileSystem::exists(settings.screenCapture.outputDirectory)) {
        settings.screenCapture.outputDirectory = "";
    }
    settings.screenCapture.includeAppRevision = false;
    settings.screenCapture.includeG3DRevision = false;
    settings.screenCapture.filenamePrefix = "_";

    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
}


// Called before the application loop begins.  Load data here and
// not in the constructor so that common exceptions will be
// automatically caught.
void App::onInit() {
    GApp::onInit();

    debugPrintf("Target frame rate = %f Hz\n", 1.0f / realTimeTargetDuration());


    setFrameDuration(1.0f / 240.0f);

    // Call setScene(shared_ptr<Scene>()) or setScene(MyScene::create()) to replace
    // the default scene here.
    
    showRenderingStats      = false;

    loadScene(

#       ifndef G3D_DEBUG
        "G3D Sponza"
#       else
        "G3D Simple Cornell Box (Area Light)" // Load something simple
#       endif
        );

    createStaircaseSceneFile();
    createSpiralLandscapeSceneFile();


    // Make the GUI after the scene is loaded because loading/rendering/simulation initialize
    // some variables that advanced GUIs may wish to reference with pointers.
    makeGUI();
}


void App::makeGUI() {
    debugWindow->setVisible(true);
    developerWindow->videoRecordDialog->setEnabled(true);
    GuiPane* infoPane = debugPane->addPane("Info", GuiTheme::ORNATE_PANE_STYLE);
    
    // Example of how to add debugging controls
    infoPane->addLabel("You can add GUI controls");
    infoPane->addLabel("in App::onInit().");
    infoPane->addButton("Exit", [this]() { m_endProgram = true; });
    infoPane->pack();

    GuiPane* rendererPane = debugPane->addPane("DefaultRenderer", GuiTheme::ORNATE_PANE_STYLE);

    // showInTextureBrowser("G3D::GBuffer/CS_NORMAL");

    GuiCheckBox* deferredBox = rendererPane->addCheckBox("Deferred Shading",
        Pointer<bool>([&]() {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                return r && r->deferredShading();
            },
            [&](bool b) {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                if (r) { r->setDeferredShading(b); }
            }));
    rendererPane->addCheckBox("Order-Independent Transparency",
        Pointer<bool>([&]() {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                return r && r->orderIndependentTransparency();
            },
            [&](bool b) {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                if (r) { r->setOrderIndependentTransparency(b); }
            }));

    GuiPane* giPane = rendererPane->addPane("Ray Tracing", GuiTheme::SIMPLE_PANE_STYLE);
 

    giPane->addCheckBox("Diffuse",
		Pointer<bool>([&]() {
			const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
			return r && r->enableDiffuseGI();
			},
			[&](bool b) {
				const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
				if (r) { r->setEnableDiffuseGI(b); }
			}));
    giPane->addCheckBox("Glossy",
        Pointer<bool>([&]() {
            const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
            return r && r->enableGlossyGI();
            },
            [&](bool b) {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                if (r) { r->setEnableGlossyGI(b); }
            }));
    giPane->addCheckBox("Show Probes",
        Pointer<bool>([&]() {
            const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
            if (notNull(r)) {
                bool allEnabled = r->m_ddgiVolumeArray.size() > 0;
                for (int i = 0; i < r->m_ddgiVolumeArray.size(); ++i) {
                    allEnabled = allEnabled && r->m_showProbeLocations[i];
                }
                return allEnabled;
            }
            return false;
            },
            [&](bool b) {
                const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
                if (notNull(r)) {
                    for (int i = 0; i < r->m_ddgiVolumeArray.size(); ++i) {
                        r->m_showProbeLocations[i] = b;
                    }
                }
            }), GuiTheme::TOOL_CHECK_BOX_STYLE);
            giPane->pack();

    giPane->moveRightOf(deferredBox);
    giPane->moveBy(100, 0);

    rendererPane->moveRightOf(infoPane);
    rendererPane->moveBy(10, 0);

    // More examples of debugging GUI controls:
    // debugPane->addCheckBox("Use explicit checking", &explicitCheck);
    // debugPane->addTextBox("Name", &myName);
    // debugPane->addNumberBox("height", &height, "m", GuiTheme::LINEAR_SLIDER, 1.0f, 2.5f);
    // button = debugPane->addButton("Run Simulator");
    // debugPane->addButton("Generate Heightfield", [this](){ generateHeightfield(); });
    // debugPane->addButton("Generate Heightfield", [this](){ makeHeightfield(imageName, scale, "model/heightfield.off"); });

    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}


// This default implementation is a direct copy of GApp::onGraphics3D to make it easy
// for you to modify. If you aren't changing the hardware rendering strategy, you can
// delete this override entirely.
void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {
    if (! scene()) {
        if ((submitToDisplayMode() == SubmitToDisplayMode::MAXIMIZE_THROUGHPUT) && (!rd->swapBuffersAutomatically())) {
            swapBuffers();
        }
        rd->clear();
        rd->pushState(); {
            rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
            drawDebugShapes();
        } rd->popState();
        return;
    }

    BEGIN_PROFILER_EVENT("App::onGraphics3D");
    GBuffer::Specification gbufferSpec = m_gbufferSpecification;

    extendGBufferSpecification(gbufferSpec);
    m_gbuffer->setSpecification(gbufferSpec);
    m_gbuffer->resize(m_framebuffer->width(), m_framebuffer->height());
    m_gbuffer->prepare(rd, activeCamera(), 0, -(float)previousSimTimeStep(), m_settings.hdrFramebuffer.depthGuardBandThickness, m_settings.hdrFramebuffer.colorGuardBandThickness);
    debugAssertGLOk();

    m_renderer->render(rd,
        activeCamera(),
        m_framebuffer,
        scene()->lightingEnvironment().ambientOcclusionSettings.enabled ? m_depthPeelFramebuffer : nullptr,
        scene()->lightingEnvironment(), m_gbuffer,
        allSurfaces,
        [&]() -> decltype(auto) { return scene()->tritree(); }); // decltype(auto) for correct return type deduction in the lambda.

    // Debug visualizations and post-process effects
    rd->pushState(m_framebuffer); {
        // Call to make the App show the output of debugDraw(...)
        rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
        drawDebugShapes();
        const shared_ptr<Entity>& selectedEntity = (notNull(developerWindow) && notNull(developerWindow->sceneEditorWindow)) ? developerWindow->sceneEditorWindow->selectedEntity() : nullptr;
        scene()->visualize(rd, selectedEntity, allSurfaces, sceneVisualizationSettings(), activeCamera());

        onPostProcessHDR3DEffects(rd);
    } rd->popState();

    // We're about to render to the actual back buffer, so swap the buffers now.
    // This call also allows the screenshot and video recording to capture the
    // previous frame just before it is displayed.
    if (submitToDisplayMode() == SubmitToDisplayMode::MAXIMIZE_THROUGHPUT) {
        swapBuffers();
    }

    // Clear the entire screen (needed even though we'll render over it, since
    // AFR uses clear() to detect that the buffer is not re-used.)
    rd->clear();

    // Perform gamma correction, bloom, and AA, and write to the native window frame buffer
    m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0), 
        settings().hdrFramebuffer.trimBandThickness().x,
        settings().hdrFramebuffer.depthGuardBandThickness.x,
        Texture::opaqueBlackIfNull(notNull(m_gbuffer) ? m_gbuffer->texture(GBuffer::Field::SS_POSITION_CHANGE) : nullptr),
        activeCamera()->jitterMotion());
    END_PROFILER_EVENT();
}


void App::onAI() {
    GApp::onAI();
    // Add non-simulation game logic and AI code here
}


void App::onNetwork() {
    GApp::onNetwork();
    // Poll net messages here
}


void App::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
    GApp::onSimulation(rdt, sdt, idt);

    // Example GUI dynamic layout code.  Resize the debugWindow to fill
    // the screen horizontally.
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}


bool App::onEvent(const GEvent& event) {
    // Handle super-class events
    if (GApp::onEvent(event)) { return true; }

    // If you need to track individual UI events, manage them here.
    // Return true if you want to prevent other parts of the system
    // from observing this specific event.
    //
    // For example,
    // if ((event.type == GEventType::GUI_ACTION) && (event.gui.control == m_button)) { ... return true; }
    // if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == GKey::TAB)) { ... return true; }
    // if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == 'p')) { ... return true; }

    if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == 'p')) { 
        const shared_ptr<DefaultRenderer>& r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
        r->setDeferredShading(! r->deferredShading());
        return true; 
    }

    return false;
}


void App::onUserInput(UserInput* ui) {
    GApp::onUserInput(ui);
    (void)ui;
    // Add key handling here based on the keys currently held or
    // ones that changed in the last frame.
}


void App::onPose(Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) {
    GApp::onPose(surface, surface2D);

    // Append any models to the arrays that you want to later be rendered by onGraphics()
}


void App::onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction.
    Surface2D::sortAndRender(rd, posed2D);
}


void App::onCleanup() {
    // Called after the application loop ends.  Place a majority of cleanup code
    // here instead of in the constructor so that exceptions can be caught.
}

// Creates a .Scene.Any file for a curving staircase
void App::createStaircaseSceneFile()
{
    G3D::TextOutput stairs("stairCase.Scene.Any");

    stairs.printf(
        "/* -*- c++ -*- */\n"
        "{\n"
        "\tname = \"StairCase\"; \n "
        "\tmodels = { \n"
        "\t\tstairModel = ArticulatedModel::Specification { \n"
        "\t\t\tfilename = \"model/crate/crate.obj\"; \n"
        "\t\t\tpreprocess = { \n"
        "\t\t\t\tsetMaterial(all(), \n"
        "\t\t\t\t\tUniversalMaterial::Specification { \n"
        "\t\t\t\t\t\tlambertian = \"wood.png\"; \n"
        "\t\t\t\t\t}; \n"
        "\t\t\t\t); \n"
        "\t\t\t\ttransformGeometry(all(), Matrix4::scale(1.25, 0.25, 5 ) ); \n"
        "\t\t\t}; \n"
        "\t\t}; \n"
        "\t}; \n"
        "\n"
        "\tentities = { \n"
        "\t\tskybox = Skybox { \n"
        "\t\t\ttexture = \"cubemap/whiteroom/whiteroom-*.png\"; \n"
        "\t\t};"
        "\n"
        "\t\tsun = Light{ \n"
        "\t\t\tattenuation = (0, 0, 1); \n"
        "\t\t\tbulbPower = Power3(4e+006); \n"
        "\t\t\tframe = CFrame::fromXYZYPRDegrees(0, 0, 0, 0, 0, 0); \n"
        "\t\t\tshadowMapSize = Vector2int16(2048, 2048); \n"
        "\t\t\tspotHalfAngleDegrees = 5; \n"
        "\t\t\trectangular = true; \n"
        "\t\t\ttype = \"SPOT\"; \n"
        "\t\t};"
        "\n"
        "\t\tcamera = Camera{ \n"
        "\t\t\tframe = CFrame::fromXYZYPRDegrees(0,0,5); \n"
        "\t\t};"
        "\n"
    );

    // Here create the rotation staircase going upwards
    for (int i = 0; i < 50; ++i) {
        stairs.printf(
            "\t\tstair%i = VisibleEntity{ \n"
            "\t\t\tmodel = \"stairModel\"; \n"
            "\t\t\tframe = CFrame::fromXYZYPRDegrees(0, %f, 0, %i, 0, 0); \n"
            "\t\t};"
            "\n",
            i, i*0.25,i*17

        );
    }


    // Complete the closing brackets
    stairs.printf(
        "\t}; \n"
        "}; \n"
    );

    stairs.commit();
}

// Creates a .Scene.Any file for a curving staircase landscape
void App::createSpiralLandscapeSceneFile()
{
    spirals = new G3D::TextOutput("spiral.Scene.Any");

    spirals->printf(
        "/* -*- c++ -*- */\n"
        "{\n"
        "\tname = \"Spirals\"; \n "
        "\tmodels = { \n"
        "\t\tspiralWood = ArticulatedModel::Specification { \n"
        "\t\t\tfilename = \"model/crate/crate.obj\"; \n"
        "\t\t\tpreprocess = { \n"
        "\t\t\t\tsetMaterial(all(), \n"
        "\t\t\t\t\tUniversalMaterial::Specification { \n"
        "\t\t\t\t\t\tlambertian = \"wood.png\"; \n"
        "\t\t\t\t\t}; \n"
        "\t\t\t\t); \n"
        "\t\t\t\ttransformGeometry(all(), Matrix4::scale(1.25, 0.25, 5 ) ); \n"
        "\t\t\t}; \n"
        "\t\t}; \n"
        "\t\tspiralMetal = ArticulatedModel::Specification { \n"
        "\t\t\tfilename = \"model/crate/crate.obj\"; \n"
        "\t\t\tpreprocess = { \n"
        "\t\t\t\tsetMaterial(all(), \n"
        "\t\t\t\t\tUniversalMaterial::Specification { \n"
        "\t\t\t\t\t\tlambertian = \"metal.png\"; \n"
        "\t\t\t\t\t}; \n"
        "\t\t\t\t); \n"
        "\t\t\t\ttransformGeometry(all(), Matrix4::scale(1.25, 0.25, 5 ) ); \n"
        "\t\t\t}; \n"
        "\t\t}; \n"
        "\t\tflatMetal = ArticulatedModel::Specification { \n"
        "\t\t\tfilename = \"model/crate/crate.obj\"; \n"
        "\t\t\tpreprocess = { \n"
        "\t\t\t\tsetMaterial(all(), \n"
        "\t\t\t\t\tUniversalMaterial::Specification { \n"
        "\t\t\t\t\t\tlambertian = \"metal.png\"; \n"
        "\t\t\t\t\t}; \n"
        "\t\t\t\t); \n"
        "\t\t\t\ttransformGeometry(all(), Matrix4::scale(500, 0.5, 500 ) ); \n"
        "\t\t\t}; \n"
        "\t\t}; \n"
        "\t}; \n"
        "\n"
        "\tentities= { \n"
        "\t\tskybox = Skybox { \n"
        "\t\t\ttexture = \"cubemap/whiteroom/whiteroom-*.png\"; \n"
        "\t\t};"
        "\n"
        "\n"
        "\t\tcamera = Camera{ \n"
        "\t\t\tframe = CFrame::fromXYZYPRDegrees(0,5,5,180,0,0); \n"
        "\t\t};"
        "\n"
    );

    spirals->printf(
        "\t\tflatBot = VisibleEntity{ \n"
        "\t\t\tmodel = \"flatMetal\"; \n"
        "\t\t\tframe = CFrame::fromXYZYPRDegrees(0, -0.5, 0, 0, 0, 0); \n"
        "\t\t};"
        "\n"
    );

    spirals->printf(
        "\t\tflatTop = VisibleEntity{ \n"
        "\t\t\tmodel = \"flatMetal\"; \n"
        "\t\t\tframe = CFrame::fromXYZYPRDegrees(0, 15, 0, 0, 0, 0); \n"
        "\t\t};"
        "\n"
    );

    createSpirals(*spirals);


    // Complete the closing brackets
    spirals->printf(
        "\t}; \n"
        "}; \n"
    );

    spirals->commit();
}

/*
Random Number generator Utility function
*/
int randomNum(int minVal, int maxVal) {

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(minVal, maxVal);
    int random_number = distrib(gen);

    return random_number;
}

/*
Automatically create Scene for Specifcaion 4
*/
void App::createSpirals(G3D::TextOutput& incomingFile) {

    // create 30 different spirals
    for (int j = 0; j < 30; ++j) {

        int randomNumX = randomNum(0, 100);   // random position for our spirals cases, 0 to 100
        int randomNumZ = randomNum(0, 100);

        // Here create the rotation staircase going upwards
        for (int i = 0; i < 50; ++i) {
            incomingFile.printf(
                "\t\tstairW%i= VisibleEntity{ \n"
                "\t\t\tmodel= \"spiralWood\"; \n"
                "\t\t\tframe= CFrame::fromXYZYPRDegrees(%i, %f, %i, %i, 0, 0); \n"
                "\t\t};"
                "\n",
                (i * 30) + j, randomNumX, i * 0.25, randomNumZ, i * 17
            );
        }
    }

    // create 30 different spirals
    for (int j = 0; j < 30; ++j) {

        int randomNumX = randomNum(0, 100);   // random position for our spirals cases, 0 to 100
        int randomNumZ = randomNum(0, 100);

        // Here create the rotation staircase going upwards
        for (int i = 0; i < 50; ++i) {
            incomingFile.printf(
                "\t\tstairM%i= VisibleEntity{ \n"
                "\t\t\tmodel= \"spiralMetal\"; \n"
                "\t\t\tframe= CFrame::fromXYZYPRDegrees(%i, %f, %i, %i, 0, 0); \n"
                "\t\t};"
                "\n",
                (i * 30) + j, randomNumX, i * 0.25, randomNumZ, i * 9
            );
        }
    }

}



