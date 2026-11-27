#!/usr/bin/env python3
"""The signed-in home of the README's first screenshot: "For you" and "Subscriptions" filled with the latest videos
of a few French creators, as the TV client's tiles, and their thumbnails, for fake_youtube --showcase.

    python3 scripts/readme/make-showcase.py OUT_DIR

Asks YouTube's public web API (no account) for each channel's Videos tab (needs Pillow). Writes OUT_DIR/recommended.json,
OUT_DIR/subscriptions.json and OUT_DIR/img/vi/<id>/hq720.jpg.
"""
import io
import json
import os
import sys
import urllib.request

from PIL import Image

CHANNELS = [  # (name, channel id)
    ("V2F", "UCnxEvD2eagj2VgtkFL5wDHA"),
    ("Feldup", "UCQVaKQcp4OxSg1eC6SF3NTw"),
    ("Théorus", "UCvdfxZgZ939jU-21VhTNIOQ"),
    ("Underscore_", "UCWedHS9qKebauVIK2J7383g"),
    ("POTATOZ - LIVE & BEST-OF", "UCWOigXsq13-_-XM3vOajXFg"),
]
AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36"


def post(endpoint, body):
    request = {"context": {"client": {"clientName": "WEB", "clientVersion": "2.20250923.01.00", "hl": "en",
                                      "gl": "FR"}}}
    request.update(body)
    req = urllib.request.Request("https://www.youtube.com/youtubei/v1/%s?prettyPrint=false" % endpoint,
                                 json.dumps(request).encode(), {"Content-Type": "application/json",
                                                                "User-Agent": AGENT})
    with urllib.request.urlopen(req, timeout=30) as reply:
        return json.loads(reply.read())


def find(node, key, out):
    if isinstance(node, dict):
        if key in node:
            out.append(node[key])
        for value in node.values():
            find(value, key, out)
    elif isinstance(node, list):
        for value in node:
            find(value, key, out)


def videos(channel_name, channel_id, count):
    lockups = []
    find(post("browse", {"browseId": channel_id, "params": "EgZ2aWRlb3PyBgQKAjoA"}), "lockupViewModel", lockups)
    out = []
    for lockup in lockups:
        meta = lockup.get("metadata", {}).get("lockupMetadataViewModel", {})
        parts = []
        for row in meta.get("metadata", {}).get("contentMetadataViewModel", {}).get("metadataRows", []):
            parts += [p.get("text", {}).get("content", "") for p in row.get("metadataParts", [])]
        badges = []
        find(lockup.get("contentImage", {}), "thumbnailBadgeViewModel", badges)
        sources = lockup.get("contentImage", {}).get("thumbnailViewModel", {}).get("image", {}).get("sources", [])
        if not sources or len(parts) < 2 or not badges:
            continue
        out.append({"id": lockup["contentId"], "title": meta.get("title", {}).get("content", ""),
                    "channel": channel_name, "views": parts[0], "when": parts[1],
                    "duration": badges[0].get("text", ""), "picture": sources[-1]["url"]})
        if len(out) == count:
            break
    return out


def tile(video):
    line = lambda text: {"lineItemRenderer": {"text": {"simpleText": text}}}
    return {"tileRenderer": {
        "contentType": "TILE_CONTENT_TYPE_VIDEO", "contentId": video["id"],
        "header": {"tileHeaderRenderer": {
            "thumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/%s/hq720.jpg" % video["id"],
                                          "width": 720, "height": 404}]},
            "thumbnailOverlays": [{"thumbnailOverlayTimeStatusRenderer": {
                "text": {"simpleText": video["duration"]}, "style": "DEFAULT"}}]}},
        "metadata": {"tileMetadataRenderer": {
            "title": {"simpleText": video["title"]},
            "lines": [{"lineRenderer": {"items": [line(video["channel"])]}},
                      {"lineRenderer": {"items": [line(video["views"]), line("•"), line(video["when"])]}}]}},
        "onSelectCommand": {"watchEndpoint": {"videoId": video["id"]}}}}


def feed(items):
    return {"contents": {"tvBrowseRenderer": {"content": {"tvSurfaceContentRenderer": {"content": {
        "sectionListRenderer": {"contents": [{"shelfRenderer": {"content": {"horizontalListRenderer": {
            "items": [tile(v) for v in items]}}}}]}}}}}}}


def main(out):
    by_channel = [videos(name, cid, 4) for name, cid in CHANNELS]
    # For you: the most viewed first video of each channel, then the others; Subscriptions: the latest of each
    recommended = [v for i in (1, 2, 3, 0) for vs in by_channel for v in vs[i:i + 1]]
    subscriptions = [v for i in (0, 1) for vs in reversed(by_channel) for v in vs[i:i + 1]]
    os.makedirs(out, exist_ok=True)
    for name, items in (("recommended.json", recommended), ("subscriptions.json", subscriptions)):
        with open(os.path.join(out, name), "w", encoding="utf-8") as f:
            json.dump(feed(items), f, ensure_ascii=False)
    for video in recommended + subscriptions:
        path = os.path.join(out, "img", "vi", video["id"], "hq720.jpg")
        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            req = urllib.request.Request(video["picture"], headers={"User-Agent": AGENT})
            with urllib.request.urlopen(req, timeout=30) as reply:
                picture = Image.open(io.BytesIO(reply.read())).convert("RGB")  # often WebP: JPEG for the app
            picture.save(path, "JPEG", quality=90)
    print("%d recommended, %d subscriptions" % (len(recommended), len(subscriptions)))


if __name__ == "__main__":
    main(sys.argv[1])
