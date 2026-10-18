#pragma once

#include <QString>
#include <QVariantMap>

// What the HTTP API answers with that is read from ProPresenter's files (see
// httpapijson.h for the rest), kept for as long as the file it was read from is
// unchanged: a stream may ask for it again at every change of the show.
namespace httpapifiles {

// A presentation as presentation/active and presentation/{uuid} give it:
// {presentation: {id, groups, has_timeline, presentation_path, destination,
// arrangements, current_arrangement, type, is_authorized, total_cues}}. Its groups in the
// file's order, each with every one of its cues (disabled ones too, said so, which the
// app's own list of slides leaves out), and its arrangements, each with the groups it
// runs through and how many cues that is. Its id's index is `listIndex`, its place in
// the list it was found in. An empty map if the file cannot be read.
QVariantMap presentation(const QString &path, qint64 listIndex);

// A presentation's UUID, as its file has it, "" if it cannot be read. Read from the
// start of the file alone, where ProPresenter writes it (after the little that says
// which program wrote it), and so quick enough to ask of every presentation of a
// library; the whole file only if it is not there.
QString presentationUuid(const QString &path);

// Which of these files is the presentation with this UUID; "" for none
QString findPresentation(const QString &uuid, const QStringList &files);

// A library's presentations (Catalog::documentsIn's rows), as library/{library_id}
// gives them: {update_type: "all", items: [{uuid, name, index}]}
QVariantMap library(const QVariantList &rows);

// A playlist's rows (PlaylistFile's, for a playlist of presentations) as playlist/{id}
// gives them, each {id, type, is_hidden, is_pco, header_color | presentation_info,
// destination}: a header, a presentation (by its UUID, and its arrangement's name and
// UUID), or a placeholder for anything else
QVariantList playlistItems(const QVariantList &rows);
QVariantMap playlistItem(const QVariantMap &row, int index);

}
