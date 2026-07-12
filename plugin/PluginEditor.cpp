#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
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
      browser (juce::WebBrowserComponent::Options {}
                   .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
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

    // Mockup canvas is 560x360 (see mockup/index.html).
    setSize (560, 360);
}

void TagoPitchEditor::resized()
{
    browser.setBounds (getLocalBounds());
}
