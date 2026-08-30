import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR if (SCRIPT_DIR / "app/hello_app").exists() else (
    SCRIPT_DIR / "contest2026_148_langyongyunji"
)
SRC = Path(os.environ.get("VELAGUARD_ASSET_DIR", "/tmp/velaguard_new_assets"))
UI = ROOT / "app/hello_app/ui"
UI.mkdir(parents=True, exist_ok=True)

BASE = (240, 280)
SCREEN = (390, 450)
THUMB = (150, 175)
FONT_SIZE = 30

ASCII = "".join(chr(i) for i in range(0x20, 0x7f))
CHINESE_TEXT = (
    "、。一上下与中串为事人令件似信倒偏克入全冲出击则判到动助化匹即参取"
    "变口叫可后启告和唤回园地场声外姿守安定尖屏工已常幕度异式录态或"
    "户手护报持指按据接撞敏数无景暂未本机校检模止求法流测消演灵点用"
    "疑知确示秒程窗立续置老能自行规解警认记设词语跌输运返进连配醒量"
    "除险静音风高麦蓝牙彩虹雨未来触控选择发送紧急请勿松开正在我没立长流星小猫"
    "即等待救援报警返回表盘地址名称设备初始就绪周日一二三四五六，："
    "简洁"
)


def find(name):
    hits = list(SRC.rglob(name))
    if not hits:
        raise FileNotFoundError(name)
    return hits[0]


def symbol(path):
    stem = path.stem
    safe = []
    for ch in stem:
        safe.append(ch if ch.isalnum() else "_")
    return "velaguard_img_" + "".join(safe)


def resize_exact(path, size=SCREEN):
    return Image.open(path).convert("RGBA").resize(size, Image.LANCZOS)


def fit_canvas(path, size, mode="cover", fill=(0, 0, 0, 0)):
    im = Image.open(path).convert("RGBA")
    sw, sh = im.size
    tw, th = size
    scale = max(tw / sw, th / sh) if mode == "cover" else min(tw / sw, th / sh)
    nw, nh = max(1, round(sw * scale)), max(1, round(sh * scale))
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", size, fill)
    canvas.alpha_composite(im, ((tw - nw) // 2, (th - nh) // 2))
    return canvas


def scale_watch_image(path):
    im = Image.open(path).convert("RGBA")
    sw, sh = im.size
    tw = max(1, round(sw * SCREEN[0] / BASE[0]))
    th = max(1, round(sh * SCREEN[1] / BASE[1]))
    return im.resize((tw, th), Image.LANCZOS)


def write_lvgl_image(name, im):
    im = im.convert("RGBA")
    w, h = im.size
    data = bytearray()
    for r, g, b, a in im.getdata():
        data.extend([b, g, r, a])

    out = UI / f"{name}.c"
    with out.open("w", encoding="ascii", newline="\n") as f:
        f.write("#include <lvgl.h>\n\n")
        f.write(f"static const uint8_t {name}_data[] =\n{{\n")
        for i in range(0, len(data), 16):
            f.write("  " + ", ".join(f"0x{x:02x}" for x in data[i:i + 16]))
            if i + 16 < len(data):
                f.write(",")
            f.write("\n")
        f.write("};\n\n")
        f.write(f"const lv_image_dsc_t {name} =\n{{\n")
        f.write("  .header.magic = LV_IMAGE_HEADER_MAGIC,\n")
        f.write("  .header.cf = LV_COLOR_FORMAT_ARGB8888,\n")
        f.write("  .header.flags = 0,\n")
        f.write(f"  .header.w = {w},\n")
        f.write(f"  .header.h = {h},\n")
        f.write(f"  .header.stride = {w * 4},\n")
        f.write(f"  .data_size = sizeof({name}_data),\n")
        f.write(f"  .data = {name}_data,\n")
        f.write("};\n")

    print(f"{name} {w}x{h}")


def cjk_font_path():
    candidates = [
        Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
        Path("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"),
        Path("/home/aiden/openvela/apps/graphics/lvgl/lvgl/scripts/built_in_font/SimSun.woff"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("usable CJK font")


def latin_font_path():
    candidates = [
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("usable latin font")


def pack_4bpp(pixels):
    out = bytearray()
    pending = None
    for value in pixels:
        nibble = max(0, min(15, round(value * 15 / 255)))
        if pending is None:
            pending = nibble
        else:
            out.append((pending << 4) | nibble)
            pending = None
    if pending is not None:
        out.append(pending << 4)
    return out


def write_font():
    cjk_font = ImageFont.truetype(str(cjk_font_path()), FONT_SIZE)
    latin_font = ImageFont.truetype(str(latin_font_path()), FONT_SIZE)
    cjk_ascent, cjk_descent = cjk_font.getmetrics()
    latin_ascent, latin_descent = latin_font.getmetrics()
    ascent = max(cjk_ascent, latin_ascent)
    descent = max(cjk_descent, latin_descent)
    line_height = ascent + descent
    base_line = descent
    chars = ASCII + "".join(sorted(set(CHINESE_TEXT) - set(ASCII)))
    glyphs = []
    bitmap = bytearray()

    for ch in chars:
        font = latin_font if ord(ch) <= 0x7e else cjk_font
        bbox = font.getbbox(ch, anchor="ls")
        adv_w = max(1, round(font.getlength(ch) * 16))
        if bbox is None:
            glyphs.append((len(bitmap), adv_w, 0, 0, 0, 0))
            continue

        x0, y0, x1, y1 = bbox
        box_w = max(0, x1 - x0)
        box_h = max(0, y1 - y0)
        if ch == " " or box_w == 0 or box_h == 0:
            glyphs.append((len(bitmap), adv_w, 0, 0, 0, 0))
            continue

        img = Image.new("L", (box_w, box_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((-x0, -y0), ch, fill=255, font=font, anchor="ls")
        glyph_bitmap = pack_4bpp(img.getdata())
        glyphs.append((len(bitmap), adv_w, box_w, box_h, x0, -y1))
        bitmap.extend(glyph_bitmap)

    cjk = [ord(ch) for ch in chars if ord(ch) > 0x7e]
    range_start = min(cjk)
    unicode_list = [cp - range_start for cp in cjk]
    range_length = max(cjk) - range_start + 1

    out = UI / "velaguard_font_30.c"
    with out.open("w", encoding="ascii", newline="\n") as f:
        f.write("#include <lvgl.h>\n\n")
        f.write("#ifndef VELAGUARD_FONT_30\n#define VELAGUARD_FONT_30 1\n#endif\n\n")
        f.write("#if VELAGUARD_FONT_30\n\n")
        f.write("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] =\n{\n")
        if bitmap:
            for i in range(0, len(bitmap), 16):
                f.write("  " + ", ".join(f"0x{x:02x}" for x in bitmap[i:i + 16]))
                if i + 16 < len(bitmap):
                    f.write(",")
                f.write("\n")
        else:
            f.write("  0x00\n")
        f.write("};\n\n")

        f.write("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] =\n{\n")
        f.write("  {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},\n")
        for index, (bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y) in enumerate(glyphs, start=1):
            comma = "," if index < len(glyphs) else ""
            f.write(
                "  {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}%s\n"
                % (bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y, comma)
            )
        f.write("};\n\n")

        f.write("static const uint16_t unicode_list_1[] =\n{\n")
        for i in range(0, len(unicode_list), 12):
            f.write("  " + ", ".join(f"0x{x:x}" for x in unicode_list[i:i + 12]))
            if i + 12 < len(unicode_list):
                f.write(",")
            f.write("\n")
        f.write("};\n\n")

        f.write("static const lv_font_fmt_txt_cmap_t cmaps[] =\n{\n")
        f.write("  {\n")
        f.write("    .range_start = 32, .range_length = 95, .glyph_id_start = 1,\n")
        f.write("    .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,\n")
        f.write("    .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n")
        f.write("  },\n")
        f.write("  {\n")
        f.write(f"    .range_start = {range_start}, .range_length = {range_length}, .glyph_id_start = 96,\n")
        f.write("    .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL,\n")
        f.write(f"    .list_length = {len(unicode_list)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY\n")
        f.write("  }\n")
        f.write("};\n\n")

        f.write("static const lv_font_fmt_txt_dsc_t font_dsc =\n{\n")
        f.write("  .glyph_bitmap = glyph_bitmap,\n")
        f.write("  .glyph_dsc = glyph_dsc,\n")
        f.write("  .cmaps = cmaps,\n")
        f.write("  .kern_dsc = NULL,\n")
        f.write("  .kern_scale = 0,\n")
        f.write("  .cmap_num = 2,\n")
        f.write("  .bpp = 4,\n")
        f.write("  .kern_classes = 0,\n")
        f.write("  .bitmap_format = 0,\n")
        f.write("};\n\n")

        f.write("const lv_font_t velaguard_font_30 =\n{\n")
        f.write("  .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n")
        f.write("  .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n")
        f.write(f"  .line_height = {line_height},\n")
        f.write(f"  .base_line = {base_line},\n")
        f.write("  .subpx = LV_FONT_SUBPX_NONE,\n")
        f.write("  .underline_position = -2,\n")
        f.write("  .underline_thickness = 1,\n")
        f.write("  .dsc = &font_dsc,\n")
        f.write("  .fallback = NULL,\n")
        f.write("  .user_data = NULL,\n")
        f.write("};\n\n")
        f.write("#endif\n")

    print(f"font velaguard_font_30.c size={FONT_SIZE} glyphs={len(glyphs)}")


def write_alarm_frames():
    frame = fit_canvas(find("铃铛.png"), (88, 88), "contain")
    write_lvgl_image("velaguard_img_alarm_clock_0", frame)


def write_count_digits():
    font = ImageFont.truetype(str(latin_font_path()), 82)
    for digit in range(10):
        text = str(digit)
        img = Image.new("RGBA", (76, 92), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), text, font=font)
        x = (76 - (bbox[2] - bbox[0])) // 2 - bbox[0]
        y = (92 - (bbox[3] - bbox[1])) // 2 - bbox[1] - 2
        draw.text((x, y), text, fill=(255, 255, 255, 255), font=font)
        write_lvgl_image(f"velaguard_img_count_{digit}", img)


write_lvgl_image("velaguard_img_bg_alarm", resize_exact(find("Group 4251.png"), SCREEN))
write_lvgl_image("velaguard_img_ble_icon", fit_canvas(find("蓝牙ico.png"), (56, 56), "contain"))
write_lvgl_image("velaguard_img_fall_icon", fit_canvas(find("跌倒 (1).png"), (96, 96), "contain"))
write_alarm_frames()
write_count_digits()

write_lvgl_image("velaguard_img_thumb_rainbow_rain",
                 fit_canvas(find("icon_rainbow_rain_clock.png"), THUMB, "contain"))
write_lvgl_image("velaguard_img_thumb_touch_future",
                 fit_canvas(find("icon_touch_future_clock.png"), THUMB, "contain"))

rainbow_assets = [
    "icon_rainbow_rain_battery_5.png",
    "icon_rainbow_rain_blue_0.png",
    "icon_rainbow_rain_blue_1.png",
    "icon_rainbow_rain_blue_2.png",
    "icon_rainbow_rain_blue_3.png",
    "icon_rainbow_rain_blue_4.png",
    "icon_rainbow_rain_blue_5.png",
    "icon_rainbow_rain_blue_6.png",
    "icon_rainbow_rain_blue_7.png",
    "icon_rainbow_rain_blue_8.png",
    "icon_rainbow_rain_blue_9.png",
    "icon_rainbow_rain_white_0.png",
    "icon_rainbow_rain_white_1.png",
    "icon_rainbow_rain_white_2.png",
    "icon_rainbow_rain_white_3.png",
    "icon_rainbow_rain_white_4.png",
    "icon_rainbow_rain_white_5.png",
    "icon_rainbow_rain_white_6.png",
    "icon_rainbow_rain_white_7.png",
    "icon_rainbow_rain_white_8.png",
    "icon_rainbow_rain_white_9.png",
]
write_lvgl_image("velaguard_img_icon_rainbow_rain_bg",
                 scale_watch_image(find("icon_rainbow_rain_bg_meteor_sharp.png")))
for name in rainbow_assets:
    path = find(name)
    write_lvgl_image(symbol(path), scale_watch_image(path))

touch_future_assets = [
    "icon_touch_future_battery_5.png",
    "icon_touch_future_colon.png",
    "icon_touch_future_date_0.png",
    "icon_touch_future_date_1.png",
    "icon_touch_future_date_2.png",
    "icon_touch_future_date_3.png",
    "icon_touch_future_date_4.png",
    "icon_touch_future_date_5.png",
    "icon_touch_future_date_6.png",
    "icon_touch_future_date_7.png",
    "icon_touch_future_date_8.png",
    "icon_touch_future_date_9.png",
    "icon_touch_future_date_slash.png",
    "icon_touch_future_hour_0.png",
    "icon_touch_future_hour_1.png",
    "icon_touch_future_hour_2.png",
    "icon_touch_future_hour_3.png",
    "icon_touch_future_hour_4.png",
    "icon_touch_future_hour_5.png",
    "icon_touch_future_hour_6.png",
    "icon_touch_future_hour_7.png",
    "icon_touch_future_hour_8.png",
    "icon_touch_future_hour_9.png",
    "icon_touch_future_illustration.png",
    "icon_touch_future_week_0.png",
    "icon_touch_future_week_1.png",
    "icon_touch_future_week_2.png",
    "icon_touch_future_week_3.png",
    "icon_touch_future_week_4.png",
    "icon_touch_future_week_5.png",
    "icon_touch_future_week_6.png",
]
for name in touch_future_assets:
    path = find(name)
    write_lvgl_image(symbol(path), scale_watch_image(path))

write_font()
