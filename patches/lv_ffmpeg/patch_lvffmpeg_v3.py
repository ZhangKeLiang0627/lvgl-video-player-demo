import sys

path = "/home/cat/lvgl/src/libs/ffmpeg/lv_ffmpeg.c"
with open(path) as f:
    s = f.read()

edits = []

# Robust duration: fall back to the longest stream's duration when the
# container-level fmt_ctx->duration is 0 / AV_NOPTS_VALUE (custom-IO demux
# often leaves it unset even though the per-stream duration is valid).
old = '''int32_t lv_ffmpeg_player_get_duration(lv_obj_t * obj)
{
    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;
    if(!player || !player->ffmpeg_ctx) return 0;
    int64_t d = player->ffmpeg_ctx->fmt_ctx->duration;
    if(d == AV_NOPTS_VALUE) return 0;
    return (int32_t)(d / 1000);   /* microseconds -> ms */
}
'''

new = '''int32_t lv_ffmpeg_player_get_duration(lv_obj_t * obj)
{
    lv_ffmpeg_player_t * player = (lv_ffmpeg_player_t *)obj;
    if(!player || !player->ffmpeg_ctx) return 0;
    AVFormatContext * fc = player->ffmpeg_ctx->fmt_ctx;
    int64_t d = (fc->duration != AV_NOPTS_VALUE) ? fc->duration : 0;
    if(d <= 0) {                       /* fallback: longest stream duration */
        for(unsigned i = 0; i < fc->nb_streams; i++) {
            AVStream * st = fc->streams[i];
            if(st && st->duration != AV_NOPTS_VALUE && st->duration > 0) {
                int64_t sd = av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
                if(sd > d) d = sd;
            }
        }
    }
    if(d <= 0) return 0;
    return (int32_t)(d / 1000);   /* microseconds -> ms */
}
'''

edits.append((old, new))

ok = True
for i, (o, n) in enumerate(edits):
    cnt = s.count(o)
    if cnt != 1:
        print("EDIT %d FAILED: count=%d" % (i, cnt))
        ok = False
    else:
        s = s.replace(o, n, 1)

if not ok:
    print("ABORT: edit not applied")
    sys.exit(1)

with open(path, "w") as f:
    f.write(s)
print("PATCH OK")
