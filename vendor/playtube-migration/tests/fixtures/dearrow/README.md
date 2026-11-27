# DeArrow fixtures

- `branding.json`: the shape of DeArrow's `GET /api/branding?videoID=` answer (one title, one thumbnail time).
- `thumbnail.webp`: 64x36 lossy WebP, a generated colour pattern standing for a thumbnail of DeArrow's server
  (no third-party content):

      ffmpeg -f lavfi -i testsrc2=size=64x36:rate=1 -frames:v 1 -c:v libwebp -lossless 0 -q:v 80 thumbnail.webp
