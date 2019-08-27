#include <ctype.h>
#include "z80.hpp"

// 利用プログラム側で MMU (Memory Management Unit) を実装します。
// ミニマムでは 64KB の RAM と 256バイト の I/O ポートが必要です。
class MMU
{
  public:
    unsigned char RAM[0x10000];
    unsigned char IO[0x100];

    // コンストラクタで0クリアしておく
    MMU()
    {
        memset(&RAM, 0, sizeof(RAM));
        memset(&IO, 0, sizeof(IO));
    }
};

// CPUからメモリ読み込み要求発生時のコールバック
unsigned char readByte(void* arg, unsigned short addr)
{
    return ((MMU*)arg)->RAM[addr];
}

// CPUからメモリ書き込み要求発生時のコールバック
void writeByte(void* arg, unsigned short addr, unsigned char value)
{
    ((MMU*)arg)->RAM[addr] = value;
}

// 入力命令（IN）発生時のコールバック
unsigned char inPort(void* arg, unsigned char port)
{
    return ((MMU*)arg)->IO[port];
}

// 出力命令（OUT）発生時のコールバック
void outPort(void* arg, unsigned char port, unsigned char value)
{
    ((MMU*)arg)->IO[port] = value;
}

// 16進数文字列かチェック（デバッグコマンド用）
bool isHexDigit(char c)
{
    if (isdigit(c)) return true;
    char uc = toupper(c);
    return 'A' <= uc && uc <= 'F';
}

// 16進数文字列を数値に変換（デバッグコマンド用）
int hexToInt(const char* cp)
{
    int result = 0;
    while (isHexDigit(*cp)) {
        result <<= 4;
        switch (toupper(*cp)) {
            case '0': break;
            case '1': result += 1; break;
            case '2': result += 2; break;
            case '3': result += 3; break;
            case '4': result += 4; break;
            case '5': result += 5; break;
            case '6': result += 6; break;
            case '7': result += 7; break;
            case '8': result += 8; break;
            case '9': result += 9; break;
            case 'A': result += 10; break;
            case 'B': result += 11; break;
            case 'C': result += 12; break;
            case 'D': result += 13; break;
            case 'E': result += 14; break;
            case 'F': result += 15; break;
            default: result >>= 4; return result;
        }
        cp++;
    }
    return result;
}

int main()
{
    // MMUインスタンスを作成
    MMU mmu;

    // ハンドアセンブルで検証用プログラムをRAMに展開
    mmu.RAM[0x0000] = 0b01000111; // LD B, A
    mmu.RAM[0x0001] = 0b00001110; // LD C, $56
    mmu.RAM[0x0002] = 0x56;
    mmu.RAM[0x0003] = 0b01010110; // LD D, (HL)
    mmu.RAM[0x0004] = 0b11011101; // LD E, (IX+4)
    mmu.RAM[0x0005] = 0b01011110;
    mmu.RAM[0x0006] = 4;
    mmu.RAM[0x0007] = 0b11111101; // LD H, (IY+4)
    mmu.RAM[0x0008] = 0b01100110;
    mmu.RAM[0x0009] = 4;
    mmu.RAM[0x000A] = 0b01110000; // LD (HL), B
    mmu.RAM[0x000B] = 0b11011101; // LD (IX+7), A
    mmu.RAM[0x000C] = 0b01110111;
    mmu.RAM[0x000D] = 7;
    mmu.RAM[0x000E] = 0b11111101; // LD (IY+7), C
    mmu.RAM[0x000F] = 0b01110001;
    mmu.RAM[0x0010] = 7;
    mmu.RAM[0x0011] = 0b00110110; // LD (HL), 123
    mmu.RAM[0x0012] = 123;
    mmu.RAM[0x0013] = 0b11011101; // LD (IX+9), 100
    mmu.RAM[0x0014] = 0b00110110;
    mmu.RAM[0x0015] = 9;
    mmu.RAM[0x0016] = 100;
    mmu.RAM[0x0017] = 0b11111101; // LD (IY+9), 200
    mmu.RAM[0x0018] = 0b00110110;
    mmu.RAM[0x0019] = 9;
    mmu.RAM[0x001A] = 200;

    // CPUインスタンスを作成
    // コールバック、コールバック引数、デバッグ出力設定を行う
    // - コールバック引数: コールバックに渡す任意の引数（ここでは、MMUインスタンスのポインタを指定）
    // - デバッグ出力設定: 省略するかNULLを指定するとデバッグ出力しない（ここでは、標準出力を指定）
    Z80 z80(readByte, writeByte, inPort, outPort, &mmu, stdout);

    // レジスタ初期値を設定（未設定時は0）
    z80.reg.pair.A = 0x12;
    z80.reg.pair.B = 0x34;
    z80.reg.pair.L = 0x01;
    z80.reg.IY = 1;

    // ステップ実行
    char cmd[1024];
    int clocks = 0;
    printf("> ");
    while (fgets(cmd, sizeof(cmd), stdin)) {
        if (isdigit(cmd[0])) {
            // 数字が入力されたらそのクロック数CPUを実行
            int hz = atoi(cmd);
            hz = z80.execute(hz);
            if (hz < 0) break;
            clocks += hz;
        } else if ('R' == toupper(cmd[0])) {
            // レジスタダンプ
            z80.registerDump();
        } else if ('M' == toupper(cmd[0])) {
            // メモリダンプ
            int addr = 0;
            for (char* cp = &cmd[1]; *cp; cp++) {
                if (isHexDigit(*cp)) {
                    addr = hexToInt(cp);
                    break;
                }
            }
            addr &= 0xFFFF;
            z80.log("[%04X] %02X %02X %02X %02X - %02X %02X %02X %02X", addr,
                    mmu.RAM[addr],
                    mmu.RAM[(addr + 1) & 0xFFFF],
                    mmu.RAM[(addr + 2) & 0xFFFF],
                    mmu.RAM[(addr + 3) & 0xFFFF],
                    mmu.RAM[(addr + 4) & 0xFFFF],
                    mmu.RAM[(addr + 5) & 0xFFFF],
                    mmu.RAM[(addr + 6) & 0xFFFF],
                    mmu.RAM[(addr + 7) & 0xFFFF]);
        } else if ('\r' == cmd[0] || '\n' == cmd[0]) {
            break;
        }
        printf("> ");
    }
    printf("executed %dHz\n", clocks);
    return 0;
}