// Presentations made up in ProPresenter's text form for the Export to PowerPoint tests
// (tst_pptx.cpp). In a file of their own because moc, which reads tst_pptx.cpp for its
// test class, loses its way in raw strings that run over several lines and hold quotes.

#include "pptxfixtures.h"

#include <google/protobuf/text_format.h>

// A slide's cue, in ProPresenter's text form: a text box over the whole slide with these
// words, an element fill if `fill` says one, and the cue's own actions before its slide
QString cueText(const QString &id, const QString &name, bool enabled, const QString &actions, const QString &textExtra,
                const QString &fill, const QString &words, const QString &notes)
{
    return QStringLiteral(R"(
cues {
  uuid { string: "%1" }
  name: "%2"
  isEnabled: %3
  %4
  actions {
    isEnabled: true
    type: ACTION_TYPE_PRESENTATION_SLIDE
    slide {
      presentation {
        base_slide {
          size { width: 1920 height: 1080 }
          elements {
            element {
              uuid { string: "%1-E" }
              name: "Words"
              bounds { origin { x: 0 y: 0 } size { width: 1920 height: 1080 } }
              opacity: 1
              %5
              text {
                rtf_data: "{\\rtf1\\ansi{\\fonttbl\\f0 Helvetica;\\f1 MadeUpFont-CondensedBlack;}\\f0\\fs144 %6}"
                attributes { font { name: "MadeUpFont-CondensedBlack" family: "Made Up Font" } %7 }
              }
            }
          }
        }
        notes { rtf_data: "%8" }
      }
    }
  }
})").arg(id, name, enabled ? QStringLiteral("true") : QStringLiteral("false"), actions, fill, words, textExtra, notes);
}

QString backgroundAction(const QString &file)
{
    return QStringLiteral(R"(actions { isEnabled: true type: ACTION_TYPE_MEDIA media {
      element { url { absolute_string: "file:///Somewhere/else/%1" } image { drawing { natural_size { width: 192 height: 108 } } } }
      layer_type: LAYER_TYPE_BACKGROUND } })").arg(file);
}

// A presentation made up for the tests: Verse (A, and B, which is turned off) and Chorus
// (C), and an arrangement "Chorus First"; A puts a red picture behind it, which C, cut out
// of a black box, shows through its letters
rv::data::Presentation madePresentation()
{
    const QString text = QStringLiteral(R"(
name: "Made up"
cue_groups { group { uuid { string: "G1" } name: "Verse" } cue_identifiers { string: "A" } cue_identifiers { string: "B" } }
cue_groups { group { uuid { string: "G2" } name: "Chorus" } cue_identifiers { string: "C" } }
arrangements { uuid { string: "R1" } name: "Chorus First" group_identifiers { string: "G2" } group_identifiers { string: "G1" } }
)") + cueText("A", "First", true, backgroundAction("Back.png"), QString(), QString(), "Words one", "{\\\\rtf1\\\\ansi A note}")
        + cueText("B", "Second", false, QString(), QString(), QString(), "Words two")
        + cueText("C", "Third", true, QString(), "cut_out_fill { }",
                  "fill { enable: true color { alpha: 1 } }",
                  "cut out words",
                  QString())
        + cueText("D", "Unlisted", true, QString(), QString(), QString(), "Not in any group");
    rv::data::Presentation presentation;
    google::protobuf::TextFormat::ParseFromString(text.toStdString(), &presentation);
    return presentation;
}

