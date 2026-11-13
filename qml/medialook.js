.pragma library

// The part of a media file's thumbnail address that says how the media is set to look in
// its Media Inspector (as mediasettings::describe has its settings): the moment chosen for
// its picture, its crop, its turn and its flips (see src/thumbnailprovider.h, which reads
// them). Shared by the operator window (Main's thumbnailUrl) and the pictures of slides
// made for the API (SlidePictureMaker), so that the two draw a slide's media alike.
function thumbnailLook(s) {
    if (!s)
        return ""
    let look = ""
    if (s.thumbnail > 0)
        look += "&t=" + s.thumbnail
    if (s.cropOn && (s.cropLeft > 0 || s.cropRight > 0 || s.cropTop > 0 || s.cropBottom > 0))
        look += "&c=" + [s.cropLeft, s.cropRight, s.cropTop, s.cropBottom].join(",")
    if (s.rotation)
        look += "&r=" + s.rotation
    if (s.flipH || s.flipV)
        look += "&f=" + (s.flipH ? "h" : "") + (s.flipV ? "v" : "")
    return look
}
