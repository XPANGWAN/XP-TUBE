# Changelog

## 1.2.0 (2026-11-21)

- The player is tidier: one row of round icons under the progress bar, the settings (quality, speed, volume,
  picture, audio track) in their own panel, the channel's picture beside its name.
- Added HDMI-CEC support: your TV's remote works in Playtube (arrows, OK, Back), if "Enable HDMI Device Link" is on
  in the PS4's settings and your TV is compatible.
- When YouTube's anti-bot check blocks the connection, Playtube says so (the block is temporary and signing in
  does not lift it) instead of YouTube's "sign in to confirm you're not a bot", and asks YouTube much less while it
  lasts.
- What plays next by itself is no longer a video already played since Playtube was started.
- Where you are in a video is sent to your YouTube history every 2 minutes instead of every 30 seconds.
- Fixed: the videos watched no longer went into the YouTube history (YouTube now asks for more than before).
- Fixed: a video whose stream YouTube cut on the way stopped as if it had ended; it now starts again from where it
  was.
- Fixed: when a video's page did not come, nothing played after it and the player closed; the page is now asked
  again.
- Fixed: some videos filled the log with the same warning.

## 1.1.1 (2026-11-18)

- Fixed: after Playtube had been open for hours, every video played in 360p whatever the quality chosen.
- Fixed: once the update was downloaded, the text of the banner at the top of the settings went past it.

## 1.1.0 (2026-11-15)

- Fixed: some videos (a picture 1080 pixels wide, often a song over a still picture) closed Playtube a few
  seconds after they started or when they were closed.
- DeArrow: an option (off by default) replaces clickbait titles and thumbnails with the ones the DeArrow
  community chose.

## 1.0.0 (2026-11-14)

- First version of Playtube.
- Fixed: a video resumed after a pause of several minutes closed Playtube (CE-34878-0); after a very long pause
  the video now asks YouTube for new addresses and goes on from the same second.
