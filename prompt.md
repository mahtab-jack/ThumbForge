Build a Windows Flutter desktop app that provides custom **File Explorer thumbnails only for PDF and video files** using a native C++ Windows Shell Thumbnail Provider.

### PDF

* Render page 1 as the thumbnail.
* For password-protected PDFs, show a clean PDF icon with a lock overlay.
* For corrupted/unreadable PDFs, show a PDF icon with a warning overlay.

### Video

* Support MP4, MKV, AVI, MOV, WebM, WMV, FLV, M4V, TS and 3GP.
* Extract a representative video frame automatically.
* Detect the video's aspect ratio and generate the preview accordingly:

  * Landscape → landscape thumbnail
  * Portrait → portrait thumbnail
  * Square → square thumbnail
* Preserve the video's original aspect ratio; never stretch or distort the frame.
* Add a subtle play-button overlay.

### Thumbnail Controls

Create a simple Flutter interface with:

* **Apply Thumbnails** — generate/apply custom thumbnails.
* **Reset Thumbnails** — remove custom thumbnails and restore Windows' normal/default thumbnails.
* Preview area showing how the thumbnails will look.
* Separate PDF and Video enable/disable controls.
* Refresh/reload Explorer thumbnails after applying or resetting.

### Technical Requirements

* Flutter for the settings/control UI.
* Native C++ `IThumbnailProvider` for Windows Explorer integration.
* PDFium or MuPDF for PDF rendering.
* FFmpeg for video frame extraction.
* Do not change file associations or default applications.
* Keep the implementation lightweight and stable.
* If thumbnail generation fails, fall back to the normal Windows thumbnail/icon.
