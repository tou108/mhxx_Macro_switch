#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// ポート番号
static const int PORT = 8765;

// マクロ1ステップ（Android から受け取るバイナリ形式と同一）
struct MacroStep {
    u64  keys;       // HidNpadButton のビットマスク
    s32  stick_lx;   // 左スティック X  (-32767 〜 32767)
    s32  stick_ly;   // 左スティック Y
    s32  stick_rx;   // 右スティック X
    s32  stick_ry;   // 右スティック Y
    u32  duration_ms;// このステップを保持するミリ秒
    u32  release_ms; // 離した後の待機ミリ秒
} __attribute__((packed));

// 制御コマンド (1バイト)
enum Command : u8 {
    CMD_LOAD   = 0x01, // マクロデータを受信（後続にステップ数 u32 + ステップ列）
    CMD_START  = 0x02, // 実行開始（後続にループ回数 u32、0 = 無限）
    CMD_STOP   = 0x03, // 実行停止
    CMD_STATUS = 0x04, // ステータス要求
};

// ステータス応答
struct Status {
    u8  running;       // 1 = 実行中
    u32 loop_count;    // 完了したループ数
    u32 current_step;  // 現在のステップ index
} __attribute__((packed));

// ─── グローバル状態 ──────────────────────────────────────────
static MacroStep* g_steps      = nullptr;
static u32        g_step_count = 0;
static bool       g_running    = false;
static u32        g_loop_done  = 0;
static u32        g_loop_max   = 0; // 0 = 無限
static u32        g_cur_step   = 0;
static HidVibrationDeviceHandle g_vib[2];
static HidNpadFullKeyState      g_pad_state;

// ─── HID 仮想コントローラー ─────────────────────────────────
static HiddbgHdlsHandle        g_hdls_handle;
static HiddbgHdlsDeviceInfo    g_device_info;
static HiddbgHdlsState         g_hdls_state;

static void apply_step(const MacroStep& step) {
    memset(&g_hdls_state, 0, sizeof(g_hdls_state));
    g_hdls_state.battery_level             = 4;
    g_hdls_state.analog_stick_l.x          = step.stick_lx;
    g_hdls_state.analog_stick_l.y          = step.stick_ly;
    g_hdls_state.analog_stick_r.x          = step.stick_rx;
    g_hdls_state.analog_stick_r.y          = step.stick_ry;
    g_hdls_state.buttons                   = step.keys;
    hiddbgSetHdlsState(g_hdls_handle, &g_hdls_state);
    svcSleepThread((u64)step.duration_ms * 1'000'000ULL);

    // ボタンを離す
    memset(&g_hdls_state, 0, sizeof(g_hdls_state));
    g_hdls_state.battery_level = 4;
    hiddbgSetHdlsState(g_hdls_handle, &g_hdls_state);
    if (step.release_ms > 0)
        svcSleepThread((u64)step.release_ms * 1'000'000ULL);
}

// ─── マクロ実行スレッド ──────────────────────────────────────
static void macro_thread(void*) {
    g_loop_done = 0;
    g_cur_step  = 0;

    while (g_running) {
        if (g_cur_step >= g_step_count) {
            g_loop_done++;
            if (g_loop_max != 0 && g_loop_done >= g_loop_max) {
                g_running = false;
                break;
            }
            g_cur_step = 0;
        }
        apply_step(g_steps[g_cur_step]);
        g_cur_step++;
    }
}

// ─── TCP 受信ループ ──────────────────────────────────────────
static void server_loop(int client_fd) {
    while (true) {
        u8 cmd = 0;
        if (recv(client_fd, &cmd, 1, MSG_WAITALL) != 1) break;

        switch (static_cast<Command>(cmd)) {

        case CMD_LOAD: {
            u32 count = 0;
            if (recv(client_fd, &count, 4, MSG_WAITALL) != 4) return;
            count = ntohl(count);
            if (count == 0 || count > 1024) return;

            delete[] g_steps;
            g_steps      = new MacroStep[count];
            g_step_count = count;

            size_t bytes = count * sizeof(MacroStep);
            size_t got   = 0;
            while (got < bytes) {
                ssize_t r = recv(client_fd, (u8*)g_steps + got, bytes - got, 0);
                if (r <= 0) return;
                got += r;
            }
            // ACK
            u8 ack = 0xAA;
            send(client_fd, &ack, 1, 0);
            break;
        }

        case CMD_START: {
            u32 loops = 0;
            if (recv(client_fd, &loops, 4, MSG_WAITALL) != 4) return;
            g_loop_max = ntohl(loops);
            g_running  = true;

            Thread t;
            threadCreate(&t, macro_thread, nullptr, nullptr, 0x4000, 0x2C, -2);
            threadStart(&t);
            threadDetach(&t);
            break;
        }

        case CMD_STOP:
            g_running = false;
            break;

        case CMD_STATUS: {
            Status s;
            s.running      = g_running ? 1 : 0;
            s.loop_count   = htonl(g_loop_done);
            s.current_step = htonl(g_cur_step);
            send(client_fd, &s, sizeof(s), 0);
            break;
        }

        default:
            break;
        }
    }
}

// ─── メイン ─────────────────────────────────────────────────
int main() {
    // サービス初期化
    socketInitializeDefault();
    hiddbgInitialize();
    nifmInitialize(NifmServiceType_User);

    // 仮想コントローラー登録
    memset(&g_device_info, 0, sizeof(g_device_info));
    g_device_info.npad_style_set  = HidNpadStyleTag_NpadFullKey;
    g_device_info.npad_assignment = HidNpadAssignmentType_Dual;
    g_device_info.unk_x4          = 2;
    hiddbgAttachHdlsVirtualDevice(&g_hdls_handle, &g_device_info);

    // TCP サーバー起動
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    while (true) {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            server_loop(client_fd);
            close(client_fd);
        }
    }

    // クリーンアップ（通常は到達しない）
    hiddbgDetachHdlsVirtualDevice(g_hdls_handle);
    hiddbgExit();
    socketExit();
    return 0;
}
