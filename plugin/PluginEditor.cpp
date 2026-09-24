#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
// The approved mockup canvas. The web UI scales itself to any window on this
// aspect ratio (ui/src/fit.ts), so the editor can be resized freely instead of
// clipping when a host hands it a different size.
constexpr int    designWidth  = 560;
constexpr int    designHeight = 360;
constexpr double designRatio  = (double) designWidth / (double) designHeight;
constexpr int    minWidth     = 420;
constexpr int    maxWidth     = 1680;

const juce::Identifier editorWidthId { "editorWidth" };

int heightForWidth (int width) { return juce::roundToInt (width / designRatio); }

const char* mimeForExtension (const juce::String& ext)
{
    if (ext == "html") return "text/html";
    if (ext == "js")   return "text/javascript";
    if (ext == "css")  return "text/css";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "json") return "application/json";
    if (ext == "png")  return "image/png";
    if (ext == "woff2") return "font/woff2";
    return "application/octet-stream";
}
} // namespace

std::optional<juce::WebBrowserComponent::Resource> TagoPitchEditor::lookupResource (const juce::String& url)
{
    // The Vite build is bundled as a zip in BinaryData; entries are stored
    // relative to dist/ (index.html, assets/...).
    static juce::MemoryInputStream zipStream (BinaryData::webui_zip, BinaryData::webui_zipSize, false);
    static juce::ZipFile zip (zipStream);

    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    if (const auto* entry = zip.getEntry (path))
    {
        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));
        if (stream == nullptr)
            return std::nullopt;

        std::vector<std::byte> data ((size_t) stream->getTotalLength());
        stream->read (data.data(), (int) data.size());
        return juce::WebBrowserComponent::Resource { std::move (data),
                                                     mimeForExtension (path.fromLastOccurrenceOf (".", false, false)) };
    }
    return std::nullopt;
}

TagoPitchEditor::TagoPitchEditor (TagoPitchProcessor& p)
    : AudioProcessorEditor (p),
      pitchProcessor (p),
      browser (juce::WebBrowserComponent::Options {}
                   // Windows must opt in to WebView2; the default there is the legacy
                   // IE engine, which cannot run the React UI. macOS default is WKWebView.
#if JUCE_WINDOWS
                   .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options (
                       juce::WebBrowserComponent::Options::WinWebView2 {}
                           .withUserDataFolder (juce::File::getSpecialLocation (
                               juce::File::SpecialLocationType::tempDirectory)))
#else
                   .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
#endif
                   .withNativeIntegrationEnabled()
                   .withResourceProvider (lookupResource)
                   .withOptionsFrom (pitchRelay)
                   .withOptionsFrom (formantRelay)
                   .withOptionsFrom (mixRelay)
                   .withOptionsFrom (gainRelay)
                   .withOptionsFrom (bypassRelay)),
      pitchAttachment (*p.apvts.getParameter (tagopitch::param::pitch), pitchRelay, nullptr),
      formantAttachment (*p.apvts.getParameter (tagopitch::param::formant), formantRelay, nullptr),
      mixAttachment (*p.apvts.getParameter (tagopitch::param::mix), mixRelay, nullptr),
      gainAttachment (*p.apvts.getParameter (tagopitch::param::gain), gainRelay, nullptr),
      bypassAttachment (*p.apvts.getParameter (tagopitch::param::bypass), bypassRelay, nullptr)
{
    addAndMakeVisible (browser);

#if TAGOPITCH_DEV_UI
    browser.goToURL ("http://localhost:5173");
#else
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#endif

    // Resizable on a locked aspect ratio. Hosts that apply display scaling used
    // to get a window smaller than the UI and simply cut it off (reported
    // 24.09.2026 for Cakewalk Sonar and Fender Studio Pro); a resizable editor
    // lets the host negotiate a size, and the UI scales into whatever it gets.
    setResizable (true, false);

    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (designRatio);
        constrainer->setSizeLimits (minWidth, heightForWidth (minWidth),
                                    maxWidth, heightForWidth (maxWidth));
    }

    const int savedWidth = (int) pitchProcessor.apvts.state.getProperty (editorWidthId, designWidth);
    const int width      = juce::jlimit (minWidth, maxWidth, savedWidth);
    setSize (width, heightForWidth (width));

    startTimerHz (30);
}

void TagoPitchEditor::timerCallback()
{
    auto* levels = new juce::DynamicObject();
    levels->setProperty ("in", pitchProcessor.readInputPeak());
    levels->setProperty ("out", pitchProcessor.readOutputPeak());
    browser.emitEventIfBrowserIsVisible ("levels", juce::var (levels));
}

void TagoPitchEditor::resized()
{
    browser.setBounds (getLocalBounds());

    // Kept on the APVTS tree, so it rides along with getStateInformation and the
    // window comes back the size the user left it at.
    pitchProcessor.apvts.state.setProperty (editorWidthId, getWidth(), nullptr);
}
