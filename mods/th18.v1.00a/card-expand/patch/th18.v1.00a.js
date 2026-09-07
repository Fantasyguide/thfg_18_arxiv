{
  "codecaves": {
    "th18_card_table": {
      "size": "0x33cc",
      "access": "RW",
      "title": "zTableCardData[] 搬迁目标（255 行 × 0x34）"
    },
    "th18_card_jumptable": {
      "size": "0x3fc",
      "access": "RW",
      "title": "allocate_new_card 跳转表搬迁目标（255 项）"
    },
    "th18_card_unlocked": {
      "size": "0x100",
      "access": "RW",
      "title": "unlocked_cards 影子数组（256 字节，下标 = card id；DLL 填）"
    },
    "th18_card_order": {
      "size": "0x3fc",
      "access": "RW",
      "title": "显示顺序表搬迁目标（255 项；DLL 重排并追加新卡）"
    },
    "th18_card_table_patch_init": {
      "code": "fc60bf<codecave:th18_card_table>be<Rxc53c0>b9f2020000f3a5bbc5000000be<codecave:th18_card_table+b60>b90d000000f3a54b75f1bf<codecave:th18_card_jumptable>be<Rx12dac>b939000000f3a5b8<Rx11489>b9c6000000f3abbf<codecave:th18_card_order>be<Rxb3600>b939000000f3a5b839000000b9c6000000f3ab61c3",
      "export": true,
      "access": "RX",
      "title": "开机把零售表（与跳转表）拷进 codecave；BP_ce_gate 完成运行时提交与回读"
    },
    "th18_snd_cfg": {
      "size": "0x910",
      "access": "RW",
      "title": "音效 cfg 表搬迁目标（116 行 × 0x14）"
    },
    "th18_snd_names": {
      "size": "0x1a4",
      "access": "RW",
      "title": "wav 名表搬迁目标（104 项 + NULL）"
    },
    "th18_snd_slots": {
      "size": "0xae0",
      "access": "RW",
      "title": "slot 数组搬迁目标（116 × 0x18）"
    },
    "th18_snd_blobs": {
      "size": "0x1a0",
      "access": "RW",
      "title": "blob 指针数组搬迁目标（104 项；72.. 由 DLL 填语音）"
    },
    "th18_snd_patch_init": {
      "code": "fc60bf<codecave:th18_snd_cfg>be<Rxc9b80>b9a4010000f3a5b854000000bb20000000ab83c710404b75f8bf<codecave:th18_snd_names>be<Rxb47a0>b949000000f3a561c3",
      "export": true,
      "access": "RX",
      "title": "开机把零售 84 行 cfg 与 72 个 wav 名从用户的 exe 拷进 codecave，并给 32 个新行写 +0 = 槽号 的骨架（不变式 I1 / I2）"
    }
  },
  "binhacks": {
    "cardtable_start_407d72": {
      "addr": "0x407d72",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_407d7f": {
      "addr": "0x407d7f",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_407d86": {
      "addr": "0x407d86",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_407d8f": {
      "addr": "0x407d8f",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_409321": {
      "addr": "0x409321",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_40932f": {
      "addr": "0x40932f",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_409336": {
      "addr": "0x409336",
      "code": "c7450c<codecave:th18_card_table+b90>",
      "expected": "c7450c505f4c00",
      "title": "fallback | mov [ebp+0xc], K | +0xb90"
    },
    "cardtable_hit_409342": {
      "addr": "0x409342",
      "code": "05<codecave:th18_card_table+30>",
      "expected": "05f0534c00",
      "title": "hit | add eax, K | +0x30"
    },
    "cardtable_start_414412": {
      "addr": "0x414412",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_414428": {
      "addr": "0x414428",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_41442f": {
      "addr": "0x41442f",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_414439": {
      "addr": "0x414439",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_414457": {
      "addr": "0x414457",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_414468": {
      "addr": "0x414468",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_41446f": {
      "addr": "0x41446f",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_414483": {
      "addr": "0x414483",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_end_41449d": {
      "addr": "0x41449d",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4144a4": {
      "addr": "0x4144a4",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_4144ae": {
      "addr": "0x4144ae",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_4144c9": {
      "addr": "0x4144c9",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4144d9": {
      "addr": "0x4144d9",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4144e0": {
      "addr": "0x4144e0",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_4144ea": {
      "addr": "0x4144ea",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_4149b4": {
      "addr": "0x4149b4",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4149c8": {
      "addr": "0x4149c8",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4149cf": {
      "addr": "0x4149cf",
      "code": "b8<codecave:th18_card_table+b80>",
      "expected": "b8405f4c00",
      "title": "fallback | mov eax, K | +0xb80"
    },
    "cardtable_hit_4149d9": {
      "addr": "0x4149d9",
      "code": "05<codecave:th18_card_table+20>",
      "expected": "05e0534c00",
      "title": "hit | add eax, K | +0x20"
    },
    "cardtable_start_4149fc": {
      "addr": "0x4149fc",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_414a09": {
      "addr": "0x414a09",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_414a10": {
      "addr": "0x414a10",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_414a1a": {
      "addr": "0x414a1a",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_414a62": {
      "addr": "0x414a62",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_414a6f": {
      "addr": "0x414a6f",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_414a76": {
      "addr": "0x414a76",
      "code": "b8<codecave:th18_card_table+b60>",
      "expected": "b8205f4c00",
      "title": "fallback | mov eax, K | +0xb60"
    },
    "cardtable_hit_414a80": {
      "addr": "0x414a80",
      "code": "05<codecave:th18_card_table>",
      "expected": "05c0534c00",
      "title": "hit | add eax, K | +0x0"
    },
    "cardtable_start_41612f": {
      "addr": "0x41612f",
      "code": "be<codecave:th18_card_table>",
      "expected": "bec0534c00",
      "title": "start | mov esi, K | +0x0"
    },
    "cardtable_end_41616d": {
      "addr": "0x41616d",
      "code": "81fe<codecave:th18_card_table+33cc>",
      "expected": "81fe885f4c00",
      "title": "end | cmp esi, K | +0x33cc"
    },
    "cardtable_fallback_416175": {
      "addr": "0x416175",
      "code": "bb<codecave:th18_card_table+b64>",
      "expected": "bb245f4c00",
      "title": "fallback | mov ebx, K | +0xb64"
    },
    "cardtable_hit_41617f": {
      "addr": "0x41617f",
      "code": "81c3<codecave:th18_card_table+4>",
      "expected": "81c3c4534c00",
      "title": "hit | add ebx, K | +0x4"
    },
    "cardtable_start_416960": {
      "addr": "0x416960",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_41696d": {
      "addr": "0x41696d",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_416974": {
      "addr": "0x416974",
      "code": "b8<codecave:th18_card_table+b6c>",
      "expected": "b82c5f4c00",
      "title": "fallback | mov eax, K | +0xb6c"
    },
    "cardtable_hit_41697e": {
      "addr": "0x41697e",
      "code": "8d80<codecave:th18_card_table+c>",
      "expected": "8d80cc534c00",
      "title": "hit | lea eax, [eax+K] | +0xc"
    },
    "cardtable_start_416fad": {
      "addr": "0x416fad",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_416fba": {
      "addr": "0x416fba",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_416fc1": {
      "addr": "0x416fc1",
      "code": "b8<codecave:th18_card_table+b7c>",
      "expected": "b83c5f4c00",
      "title": "fallback | mov eax, K | +0xb7c"
    },
    "cardtable_hit_416fcb": {
      "addr": "0x416fcb",
      "code": "8d80<codecave:th18_card_table+1c>",
      "expected": "8d80dc534c00",
      "title": "hit | lea eax, [eax+K] | +0x1c"
    },
    "cardtable_start_416fdc": {
      "addr": "0x416fdc",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_416fe9": {
      "addr": "0x416fe9",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_416ff0": {
      "addr": "0x416ff0",
      "code": "b9<codecave:th18_card_table+b60>",
      "expected": "b9205f4c00",
      "title": "fallback | mov ecx, K | +0xb60"
    },
    "cardtable_hit_416ffa": {
      "addr": "0x416ffa",
      "code": "81c1<codecave:th18_card_table>",
      "expected": "81c1c0534c00",
      "title": "hit | add ecx, K | +0x0"
    },
    "cardtable_start_417010": {
      "addr": "0x417010",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_41701d": {
      "addr": "0x41701d",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_417024": {
      "addr": "0x417024",
      "code": "b8<codecave:th18_card_table+b70>",
      "expected": "b8305f4c00",
      "title": "fallback | mov eax, K | +0xb70"
    },
    "cardtable_hit_41702e": {
      "addr": "0x41702e",
      "code": "8d80<codecave:th18_card_table+10>",
      "expected": "8d80d0534c00",
      "title": "hit | lea eax, [eax+K] | +0x10"
    },
    "cardtable_start_417044": {
      "addr": "0x417044",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_417058": {
      "addr": "0x417058",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_41705f": {
      "addr": "0x41705f",
      "code": "b8<codecave:th18_card_table+b70>",
      "expected": "b8305f4c00",
      "title": "fallback | mov eax, K | +0xb70"
    },
    "cardtable_hit_417069": {
      "addr": "0x417069",
      "code": "8d80<codecave:th18_card_table+10>",
      "expected": "8d80d0534c00",
      "title": "hit | lea eax, [eax+K] | +0x10"
    },
    "cardtable_start_41707c": {
      "addr": "0x41707c",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_417089": {
      "addr": "0x417089",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_417090": {
      "addr": "0x417090",
      "code": "b8<codecave:th18_card_table+b74>",
      "expected": "b8345f4c00",
      "title": "fallback | mov eax, K | +0xb74"
    },
    "cardtable_hit_41709a": {
      "addr": "0x41709a",
      "code": "8d80<codecave:th18_card_table+14>",
      "expected": "8d80d4534c00",
      "title": "hit | lea eax, [eax+K] | +0x14"
    },
    "cardtable_start_4170ab": {
      "addr": "0x4170ab",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4170b8": {
      "addr": "0x4170b8",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4170bf": {
      "addr": "0x4170bf",
      "code": "b8<codecave:th18_card_table+b74>",
      "expected": "b8345f4c00",
      "title": "fallback | mov eax, K | +0xb74"
    },
    "cardtable_hit_4170c9": {
      "addr": "0x4170c9",
      "code": "8d80<codecave:th18_card_table+14>",
      "expected": "8d80d4534c00",
      "title": "hit | lea eax, [eax+K] | +0x14"
    },
    "cardtable_start_41745d": {
      "addr": "0x41745d",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_41746a": {
      "addr": "0x41746a",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_417471": {
      "addr": "0x417471",
      "code": "b8<codecave:th18_card_table+b7c>",
      "expected": "b83c5f4c00",
      "title": "fallback | mov eax, K | +0xb7c"
    },
    "cardtable_hit_41747b": {
      "addr": "0x41747b",
      "code": "8d80<codecave:th18_card_table+1c>",
      "expected": "8d80dc534c00",
      "title": "hit | lea eax, [eax+K] | +0x1c"
    },
    "cardtable_start_41748c": {
      "addr": "0x41748c",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_417499": {
      "addr": "0x417499",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4174a0": {
      "addr": "0x4174a0",
      "code": "b9<codecave:th18_card_table+b60>",
      "expected": "b9205f4c00",
      "title": "fallback | mov ecx, K | +0xb60"
    },
    "cardtable_hit_4174aa": {
      "addr": "0x4174aa",
      "code": "81c1<codecave:th18_card_table>",
      "expected": "81c1c0534c00",
      "title": "hit | add ecx, K | +0x0"
    },
    "cardtable_start_4174bc": {
      "addr": "0x4174bc",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4174c9": {
      "addr": "0x4174c9",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4174d0": {
      "addr": "0x4174d0",
      "code": "b8<codecave:th18_card_table+b74>",
      "expected": "b8345f4c00",
      "title": "fallback | mov eax, K | +0xb74"
    },
    "cardtable_hit_4174da": {
      "addr": "0x4174da",
      "code": "8d80<codecave:th18_card_table+14>",
      "expected": "8d80d4534c00",
      "title": "hit | lea eax, [eax+K] | +0x14"
    },
    "cardtable_start_4174e7": {
      "addr": "0x4174e7",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4174f8": {
      "addr": "0x4174f8",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4174ff": {
      "addr": "0x4174ff",
      "code": "b9<codecave:th18_card_table+b60>",
      "expected": "b9205f4c00",
      "title": "fallback | mov ecx, K | +0xb60"
    },
    "cardtable_hit_417509": {
      "addr": "0x417509",
      "code": "81c1<codecave:th18_card_table>",
      "expected": "81c1c0534c00",
      "title": "hit | add ecx, K | +0x0"
    },
    "cardtable_start_41754d": {
      "addr": "0x41754d",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_41755a": {
      "addr": "0x41755a",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_417561": {
      "addr": "0x417561",
      "code": "b8<codecave:th18_card_table+b7c>",
      "expected": "b83c5f4c00",
      "title": "fallback | mov eax, K | +0xb7c"
    },
    "cardtable_hit_41756b": {
      "addr": "0x41756b",
      "code": "8d80<codecave:th18_card_table+1c>",
      "expected": "8d80dc534c00",
      "title": "hit | lea eax, [eax+K] | +0x1c"
    },
    "cardtable_start_417578": {
      "addr": "0x417578",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_417588": {
      "addr": "0x417588",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_41758f": {
      "addr": "0x41758f",
      "code": "b9<codecave:th18_card_table+b60>",
      "expected": "b9205f4c00",
      "title": "fallback | mov ecx, K | +0xb60"
    },
    "cardtable_hit_417599": {
      "addr": "0x417599",
      "code": "81c1<codecave:th18_card_table>",
      "expected": "81c1c0534c00",
      "title": "hit | add ecx, K | +0x0"
    },
    "cardtable_start_4175ab": {
      "addr": "0x4175ab",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4175b8": {
      "addr": "0x4175b8",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4175bf": {
      "addr": "0x4175bf",
      "code": "b9<codecave:th18_card_table+b60>",
      "expected": "b9205f4c00",
      "title": "fallback | mov ecx, K | +0xb60"
    },
    "cardtable_hit_4175c9": {
      "addr": "0x4175c9",
      "code": "81c1<codecave:th18_card_table>",
      "expected": "81c1c0534c00",
      "title": "hit | add ecx, K | +0x0"
    },
    "cardtable_start_418e0e": {
      "addr": "0x418e0e",
      "code": "ba<codecave:th18_card_table+4>",
      "expected": "bac4534c00",
      "title": "start | mov edx, K | +0x4"
    },
    "cardtable_start_4636b2": {
      "addr": "0x4636b2",
      "code": "b8<codecave:th18_card_table+4>",
      "expected": "b8c4534c00",
      "title": "start | mov eax, K | +0x4"
    },
    "cardtable_end_4636bf": {
      "addr": "0x4636bf",
      "code": "3d<codecave:th18_card_table+33d0>",
      "expected": "3d8c5f4c00",
      "title": "end | cmp eax, K | +0x33d0"
    },
    "cardtable_fallback_4636c6": {
      "addr": "0x4636c6",
      "code": "b8<codecave:th18_card_table+b84>",
      "expected": "b8445f4c00",
      "title": "fallback | mov eax, K | +0xb84"
    },
    "cardtable_hit_4636d0": {
      "addr": "0x4636d0",
      "code": "05<codecave:th18_card_table+24>",
      "expected": "05e4534c00",
      "title": "hit | add eax, K | +0x24"
    },
    "alloc_bound_411479": {
      "addr": "0x411479",
      "code": "83fbfe",
      "expected": "83fb38",
      "title": "allocate_new_card: cmp ebx, 0x38 → 0xfe（可分配 id 上界）"
    },
    "alloc_jumptable_411482": {
      "addr": "0x411482",
      "code": "ff249d<codecave:th18_card_jumptable>",
      "expected": "ff249dac2d4100",
      "title": "allocate_new_card: jmp [0x412dac+ebx*4] → 新跳转表"
    },
    "grow_4082d6": {
      "addr": "0x4082d6",
      "code": "686c110000",
      "expected": "68700d0000",
      "title": "zAbilityManager operator_new 分配：0xd70 → 0x116c"
    },
    "grow_4082ec": {
      "addr": "0x4082ec",
      "code": "686c110000",
      "expected": "68700d0000",
      "title": "zAbilityManager operator_new 的 memset：0xd70 → 0x116c"
    },
    "grow_40860a": {
      "addr": "0x40860a",
      "code": "686c110000",
      "expected": "68700d0000",
      "title": "zAbilityManager sized delete：0xd70 → 0x116c"
    },
    "grow_407eb0": {
      "addr": "0x407eb0",
      "code": "8dbb700d0000",
      "expected": "8dbb840c0000",
      "title": "reset_cards：lea edi,[mgr+owned]"
    },
    "grow_407eb6": {
      "addr": "0x407eb6",
      "code": "b9ff000000",
      "expected": "b938000000",
      "title": "reset_cards：rep stosd 项数 56 → 255"
    },
    "grow_412d42": {
      "addr": "0x412d42",
      "code": "c78487700d000001000000",
      "expected": "c78487840c000001000000",
      "title": "allocate_new_card 尾段：owned[id] = 1"
    },
    "grow_416f8f": {
      "addr": "0x416f8f",
      "code": "b9700d0000",
      "expected": "b9840c0000",
      "title": "商店循环起点 → +0xd70"
    },
    "grow_41744a": {
      "addr": "0x41744a",
      "code": "bb700d0000",
      "expected": "bb840c0000",
      "title": "商店循环起点 → +0xd70"
    },
    "grow_417535": {
      "addr": "0x417535",
      "code": "bb700d0000",
      "expected": "bb840c0000",
      "title": "商店循环起点 → +0xd70"
    },
    "grow_41716b": {
      "addr": "0x41716b",
      "code": "81f96c110000",
      "expected": "81f9640d0000",
      "title": "商店循环上界 → +0x116c（255 个 id；幻影由 NULL/BACK 行 +0x14=6 排除）"
    },
    "grow_417527": {
      "addr": "0x417527",
      "code": "81fb6c110000",
      "expected": "81fb640d0000",
      "title": "商店循环上界 → +0x116c（255 个 id；幻影由 NULL/BACK 行 +0x14=6 排除）"
    },
    "grow_4175e7": {
      "addr": "0x4175e7",
      "code": "81fb6c110000",
      "expected": "81fb640d0000",
      "title": "商店循环上界 → +0x116c（255 个 id；幻影由 NULL/BACK 行 +0x14=6 排除）"
    },
    "unlock_41440b": {
      "addr": "0x41440b",
      "code": "388a<codecave:th18_card_unlocked>90",
      "expected": "388c1088f50500",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,r8 [eax+edx+0x5f588] → [edx+SHADOW]（eax = 存档指针）"
    },
    "unlock_4149ec": {
      "addr": "0x4149ec",
      "code": "80be<codecave:th18_card_unlocked>0090",
      "expected": "80bc0688f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [esi+eax+0x5f588] → [esi+SHADOW]（eax = 存档指针）"
    },
    "unlock_416590": {
      "addr": "0x416590",
      "code": "80bb<codecave:th18_card_unlocked>0090",
      "expected": "80bc1888f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [eax+ebx+0x5f588] → [ebx+SHADOW]（eax = 存档指针）"
    },
    "unlock_41694e": {
      "addr": "0x41694e",
      "code": "80ba<codecave:th18_card_unlocked>0090",
      "expected": "80bc1088f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [eax+edx+0x5f588] → [edx+SHADOW]（eax = 存档指针）"
    },
    "unlock_416e3d": {
      "addr": "0x416e3d",
      "code": "3882<codecave:th18_card_unlocked>90",
      "expected": "38840a88f50500",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,r8 [edx+ecx+0x5f588] → [edx+SHADOW]（ecx = 存档指针）"
    },
    "unlock_417125": {
      "addr": "0x417125",
      "code": "80be<codecave:th18_card_unlocked>0090",
      "expected": "80bc3088f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [eax+esi+0x5f588] → [esi+SHADOW]（eax = 存档指针）"
    },
    "unlock_417ea3": {
      "addr": "0x417ea3",
      "code": "80b9<codecave:th18_card_unlocked>0090",
      "expected": "80bc0188f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [ecx+eax+0x5f588] → [ecx+SHADOW]（eax = 存档指针）"
    },
    "unlock_418df6": {
      "addr": "0x418df6",
      "code": "80bf<codecave:th18_card_unlocked>0090",
      "expected": "80bc3e88f5050000",
      "title": "unlocked_cards 读 → 影子数组：cmp m8,imm8 [esi+edi+0x5f588] → [edi+SHADOW]（esi = 存档指针）"
    },
    "unlock_418e15": {
      "addr": "0x418e15",
      "code": "8a80<codecave:th18_card_unlocked>90",
      "expected": "8a840688f50500",
      "title": "unlocked_cards 读 → 影子数组：mov r8,m8 [esi+eax+0x5f588] → [eax+SHADOW]（esi = 存档指针）"
    },
    "order_414401": {
      "addr": "0x414401",
      "code": "8b1495<codecave:th18_card_order>",
      "expected": "8b149500364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "order_4145f8": {
      "addr": "0x4145f8",
      "code": "8b0c8d<codecave:th18_card_order>",
      "expected": "8b0c8d00364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "order_414639": {
      "addr": "0x414639",
      "code": "ff3485<codecave:th18_card_order>",
      "expected": "ff348500364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "order_41499f": {
      "addr": "0x41499f",
      "code": "b8<codecave:th18_card_order>",
      "expected": "b800364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "order_414b54": {
      "addr": "0x414b54",
      "code": "3d<codecave:th18_card_order+3fc>",
      "expected": "3de4364b00",
      "title": "显示顺序表 → codecave（尾界 = 255 项）"
    },
    "order_415681": {
      "addr": "0x415681",
      "code": "8b0c85<codecave:th18_card_order>",
      "expected": "8b0c8500364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "order_4156b6": {
      "addr": "0x4156b6",
      "code": "ff3485<codecave:th18_card_order>",
      "expected": "ff348500364b00",
      "title": "显示顺序表 → codecave（引用）"
    },
    "menu_413817": {
      "addr": "0x413817",
      "code": "68f8170000",
      "expected": "68fc130000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_413831": {
      "addr": "0x413831",
      "code": "68f8170000",
      "expected": "68fc130000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_413abb": {
      "addr": "0x413abb",
      "code": "68f8170000",
      "expected": "68fc130000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_41495c": {
      "addr": "0x41495c",
      "code": "bfff000000",
      "expected": "bf38000000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_4145d2": {
      "addr": "0x4145d2",
      "code": "8986100c0000",
      "expected": "898618fbffff",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_414b3f": {
      "addr": "0x414b3f",
      "code": "89b010080000",
      "expected": "89b018f7ffff",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_414b81": {
      "addr": "0x414b81",
      "code": "8d87fc130000",
      "expected": "8d8704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_414beb": {
      "addr": "0x414beb",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_414e9f": {
      "addr": "0x414e9f",
      "code": "8b8c8ffc130000",
      "expected": "8b8c8f04030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_414eba": {
      "addr": "0x414eba",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_415049": {
      "addr": "0x415049",
      "code": "8d87fc130000",
      "expected": "8d8704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_415115": {
      "addr": "0x415115",
      "code": "8a8c8ffc130000",
      "expected": "8a8c8f04030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_415129": {
      "addr": "0x415129",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_41514a": {
      "addr": "0x41514a",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_4151ef": {
      "addr": "0x4151ef",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_41520c": {
      "addr": "0x41520c",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_4152b4": {
      "addr": "0x4152b4",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_4152d5": {
      "addr": "0x4152d5",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_415868": {
      "addr": "0x415868",
      "code": "ffb487fc130000",
      "expected": "ffb48704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "menu_415e83": {
      "addr": "0x415e83",
      "code": "8d87fc130000",
      "expected": "8d8704030000",
      "title": "zAbilityMenu 扩容 / __card_ids → +0x13fc"
    },
    "snd_40111a": {
      "addr": "0x40111a",
      "code": "b8<codecave:th18_snd_cfg>",
      "expected": "b8809b4c00",
      "title": "音效表扩容：pre-main 扫描起点"
    },
    "snd_476457": {
      "addr": "0x476457",
      "code": "b8<codecave:th18_snd_cfg>",
      "expected": "b8809b4c00",
      "title": "音效表扩容：init 循环1 扫描起点"
    },
    "snd_4766bd": {
      "addr": "0x4766bd",
      "code": "be<codecave:th18_snd_cfg+4>",
      "expected": "be849b4c00",
      "title": "音效表扩容：init 循环2 游标（行+4）"
    },
    "snd_476716": {
      "addr": "0x476716",
      "code": "8b0485<codecave:th18_snd_cfg+4>",
      "expected": "8b0485849b4c00",
      "title": "音效表扩容：init 错误路径取 wav 名下标"
    },
    "snd_476bea": {
      "addr": "0x476bea",
      "code": "0fbf3485<codecave:th18_snd_cfg+a>",
      "expected": "0fbf34858a9b4c00",
      "title": "音效表扩容：play_sound 两参版取音量"
    },
    "snd_476c8d": {
      "addr": "0x476c8d",
      "code": "0fbf3c85<codecave:th18_snd_cfg+a>",
      "expected": "0fbf3c858a9b4c00",
      "title": "音效表扩容：play_sound 三参版取音量"
    },
    "snd_4766ef": {
      "addr": "0x4766ef",
      "code": "81fe<codecave:th18_snd_cfg+914>",
      "expected": "81fe14a24c00",
      "title": "音效表扩容：init 循环2 尾界（★ 游标是行+4）"
    },
    "snd_401110": {
      "addr": "0x401110",
      "code": "c782<codecave:th18_snd_slots+4>ffffffff",
      "expected": "c78208c85600ffffffff",
      "title": "音效表扩容：pre-main 写 +4 = -1"
    },
    "snd_401129": {
      "addr": "0x401129",
      "code": "898a<codecave:th18_snd_slots+c>",
      "expected": "898a10c85600",
      "title": "音效表扩容：pre-main 写 +0xc = 槽号"
    },
    "snd_401130": {
      "addr": "0x401130",
      "code": "8982<codecave:th18_snd_slots+8>",
      "expected": "89820cc85600",
      "title": "音效表扩容：pre-main 写 +8 = &cfg 行"
    },
    "snd_444d8f": {
      "addr": "0x444d8f",
      "code": "be<codecave:th18_snd_slots>",
      "expected": "be04c85600",
      "title": "音效表扩容：stop-all 起点"
    },
    "snd_45a4a1": {
      "addr": "0x45a4a1",
      "code": "be<codecave:th18_snd_slots+8>",
      "expected": "be0cc85600",
      "title": "音效表扩容：设备恢复重播循环起点（+8 游标）"
    },
    "snd_45ff38": {
      "addr": "0x45ff38",
      "code": "8b0d<codecave:th18_snd_slots+1e0>",
      "expected": "8b0de4c95600",
      "title": "音效表扩容：★ 硬编码槽 20（se_lazer02 常驻激光音）"
    },
    "snd_471393": {
      "addr": "0x471393",
      "code": "be<codecave:th18_snd_slots>",
      "expected": "be04c85600",
      "title": "音效表扩容：WinMain 释放起点"
    },
    "snd_4766b8": {
      "addr": "0x4766b8",
      "code": "bf<codecave:th18_snd_slots>",
      "expected": "bf04c85600",
      "title": "音效表扩容：init 循环2 起点"
    },
    "snd_476c60": {
      "addr": "0x476c60",
      "code": "8934c5<codecave:th18_snd_slots+4>",
      "expected": "8934c508c85600",
      "title": "音效表扩容：play_sound 两参版写音量"
    },
    "snd_476d06": {
      "addr": "0x476d06",
      "code": "893cc5<codecave:th18_snd_slots+4>",
      "expected": "893cc508c85600",
      "title": "音效表扩容：play_sound 三参版写音量"
    },
    "snd_477533": {
      "addr": "0x477533",
      "code": "8b14f5<codecave:th18_snd_slots>",
      "expected": "8b14f504c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_477562": {
      "addr": "0x477562",
      "code": "8b04f5<codecave:th18_snd_slots>",
      "expected": "8b04f504c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_4775aa": {
      "addr": "0x4775aa",
      "code": "8b14fd<codecave:th18_snd_slots>",
      "expected": "8b14fd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_4775bf": {
      "addr": "0x4775bf",
      "code": "8b04fd<codecave:th18_snd_slots>",
      "expected": "8b04fd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_4775ce": {
      "addr": "0x4775ce",
      "code": "8b04fd<codecave:th18_snd_slots>",
      "expected": "8b04fd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_4775fa": {
      "addr": "0x4775fa",
      "code": "8b0cfd<codecave:th18_snd_slots>",
      "expected": "8b0cfd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_477652": {
      "addr": "0x477652",
      "code": "8b04fd<codecave:th18_snd_slots>",
      "expected": "8b04fd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_47766b": {
      "addr": "0x47766b",
      "code": "8b0cfd<codecave:th18_snd_slots>",
      "expected": "8b0cfd04c85600",
      "title": "音效表扩容：消费者读 buffer"
    },
    "snd_4777c0": {
      "addr": "0x4777c0",
      "code": "ff34c5<codecave:th18_snd_slots>",
      "expected": "ff34c504c85600",
      "title": "音效表扩容：DuplicateSoundBuffer 源"
    },
    "snd_4775f3": {
      "addr": "0x4775f3",
      "code": "8b04fd<codecave:th18_snd_slots+8>",
      "expected": "8b04fd0cc85600",
      "title": "音效表扩容：消费者读 &cfg 行"
    },
    "snd_477664": {
      "addr": "0x477664",
      "code": "8b04fd<codecave:th18_snd_slots+8>",
      "expected": "8b04fd0cc85600",
      "title": "音效表扩容：消费者读 &cfg 行"
    },
    "snd_477736": {
      "addr": "0x477736",
      "code": "b9<codecave:th18_snd_slots+8>",
      "expected": "b90cc85600",
      "title": "音效表扩容：0x4776f0 去重扫描起点"
    },
    "snd_4775dc": {
      "addr": "0x4775dc",
      "code": "8934fd<codecave:th18_snd_slots+10>",
      "expected": "8934fd14c85600",
      "title": "音效表扩容：消费者写播放态"
    },
    "snd_47753a": {
      "addr": "0x47753a",
      "code": "c704f5<codecave:th18_snd_slots+14>00000000",
      "expected": "c704f518c8560000000000",
      "title": "音效表扩容：消费者清播放位"
    },
    "snd_47755b": {
      "addr": "0x47755b",
      "code": "8904f5<codecave:th18_snd_slots+14>",
      "expected": "8904f518c85600",
      "title": "音效表扩容：消费者写播放位"
    },
    "snd_444dbf": {
      "addr": "0x444dbf",
      "code": "81fe<codecave:th18_snd_slots+ae0>",
      "expected": "81fee4cf5600",
      "title": "音效表扩容：stop-all 尾界"
    },
    "snd_4713ad": {
      "addr": "0x4713ad",
      "code": "81fe<codecave:th18_snd_slots+ae0>",
      "expected": "81fee4cf5600",
      "title": "音效表扩容：WinMain 释放尾界"
    },
    "snd_45a4c5": {
      "addr": "0x45a4c5",
      "code": "81fe<codecave:th18_snd_slots+ae8>",
      "expected": "81feeccf5600",
      "title": "音效表扩容：重播循环尾界（+8 游标）"
    },
    "snd_4713b5": {
      "addr": "0x4713b5",
      "code": "be<codecave:th18_snd_blobs>",
      "expected": "bee4cf5600",
      "title": "音效表扩容：WinMain 释放起点"
    },
    "snd_4767cc": {
      "addr": "0x4767cc",
      "code": "8904b5<codecave:th18_snd_blobs>",
      "expected": "8904b5e4cf5600",
      "title": "音效表扩容：预加载线程写 blob"
    },
    "snd_477758": {
      "addr": "0x477758",
      "code": "833c85<codecave:th18_snd_blobs>00",
      "expected": "833c85e4cf560000",
      "title": "音效表扩容：0x4776f0 判空"
    },
    "snd_47777b": {
      "addr": "0x47777b",
      "code": "833c85<codecave:th18_snd_blobs>00",
      "expected": "833c85e4cf560000",
      "title": "音效表扩容：0x4776f0 判空（等待中）"
    },
    "snd_477788": {
      "addr": "0x477788",
      "code": "8b3c85<codecave:th18_snd_blobs>",
      "expected": "8b3c85e4cf5600",
      "title": "音效表扩容：0x4776f0 取 blob"
    },
    "snd_477905": {
      "addr": "0x477905",
      "code": "8b0485<codecave:th18_snd_blobs>",
      "expected": "8b0485e4cf5600",
      "title": "音效表扩容：0x4776f0 取 blob"
    },
    "snd_47791f": {
      "addr": "0x47791f",
      "code": "c70485<codecave:th18_snd_blobs>00000000",
      "expected": "c70485e4cf560000000000",
      "title": "音效表扩容：0x4776f0 清 blob"
    },
    "snd_477956": {
      "addr": "0x477956",
      "code": "8b0485<codecave:th18_snd_blobs>",
      "expected": "8b0485e4cf5600",
      "title": "音效表扩容：0x4776f0 取 blob"
    },
    "snd_477970": {
      "addr": "0x477970",
      "code": "c70485<codecave:th18_snd_blobs>00000000",
      "expected": "c70485e4cf560000000000",
      "title": "音效表扩容：0x4776f0 清 blob"
    },
    "snd_4713d8": {
      "addr": "0x4713d8",
      "code": "81fe<codecave:th18_snd_blobs+120>",
      "expected": "81fe04d15600",
      "title": "音效表扩容：WinMain 释放尾界（★ 只到零售 72）"
    },
    "snd_4766d3": {
      "addr": "0x4766d3",
      "code": "ff3485<codecave:th18_snd_names>",
      "expected": "ff3485a0474b00",
      "title": "音效表扩容：init 加载取名"
    },
    "snd_47671d": {
      "addr": "0x47671d",
      "code": "ff3485<codecave:th18_snd_names>",
      "expected": "ff3485a0474b00",
      "title": "音效表扩容：init 错误路径取名"
    },
    "snd_4767bc": {
      "addr": "0x4767bc",
      "code": "8b0cb5<codecave:th18_snd_names>",
      "expected": "8b0cb5a0474b00",
      "title": "音效表扩容：预加载取名"
    },
    "snd_476803": {
      "addr": "0x476803",
      "code": "ff34b5<codecave:th18_snd_names>",
      "expected": "ff34b5a0474b00",
      "title": "音效表扩容：预加载错误路径取名"
    },
    "snd_401139": {
      "addr": "0x401139",
      "code": "81fae00a0000",
      "expected": "81fae0070000",
      "title": "音效表扩容：pre-main slot 初始化字节界"
    },
    "snd_476472": {
      "addr": "0x476472",
      "code": "83fa74",
      "expected": "83fa54",
      "title": "音效表扩容：init 循环1 槽数界"
    }
  },
  "breakpoints": {
    "ce_gate": {
      "addr": "0x4637d0",
      "cavesize": 5,
      "expected": "558bec6aff",
      "title": "自检门：ScoreFile__load 入口 → BP_ce_gate（填表 + 回读验证 + 写日志）"
    },
    "ce_unlock_write": {
      "addr": "0x418e04",
      "cavesize": 8,
      "expected": "c6843e88f5050001",
      "title": "mark_obtained 的写 → BP_ce_unlock_write：影子[id]=1；id<57 放行原指令写零售存档，否则写 side-car"
    },
    "ce_save_loaded": {
      "addr": "0x46398a",
      "cavesize": 6,
      "expected": "8db3b8f40500",
      "title": "ScoreFile__load 尾段 → BP_ce_save_loaded：影子[0..56] ← 零售存档，[57..] ← side-car"
    },
    "ce_unlock_all": {
      "addr": "0x4648fe",
      "cavesize": 6,
      "expected": "8d8388f50500",
      "title": "ScoreFile__unlock_all → BP_ce_unlock_all：影子[0..55]=1（镜像紧接着的 memset）"
    },
    "ce_text_name": {
      "addr": "0x416694",
      "cavesize": 6,
      "expected": "69cbc0010000",
      "title": "文案重定向 → BP_ce_text_name：id<57 照算 id*0x1c0，否则指向 DLL 的扩展文案缓冲（FUN_00416540 卡名：imul ecx, ebx, 0x1c0）"
    },
    "ce_text_desc": {
      "addr": "0x416779",
      "cavesize": 7,
      "expected": "69450cc0010000",
      "title": "文案重定向 → BP_ce_text_desc：id<57 照算 id*0x1c0，否则指向 DLL 的扩展文案缓冲（FUN_00416540 说明 6 行：imul eax, [ebp+0xc], 0x1c0）"
    },
    "ce_text_notify": {
      "addr": "0x41926a",
      "cavesize": 6,
      "expected": "69c3c0010000",
      "title": "文案重定向 → BP_ce_text_notify：id<57 照算 id*0x1c0，否则指向 DLL 的扩展文案缓冲（获得通知：imul eax, ebx, 0x1c0）"
    },
    "ce_card_bind": {
      "addr": "0x412cec",
      "cavesize": 6,
      "expected": "895e048b4728",
      "title": "行为 SDK → BP_ce_card_bind：allocate_new_card 公共尾段 mov [esi+4],ebx（esi=卡对象, ebx=id）+ mov eax,[edi+0x28]：登记了行为的 id 换虚表"
    },
    "ce_item_score": {
      "addr": "0x446cf6",
      "cavesize": 6,
      "expected": "8d872c0c0000",
      "title": "行为 SDK → BP_ce_item_score：collect_money_item：esi=道具身价，弹窗与计分之前；沿卡链表调 on_item_score(&esi)"
    },
    "ce_item_money": {
      "addr": "0x446d28",
      "cavesize": 6,
      "expected": "ff0530cd4c00",
      "title": "行为 SDK → BP_ce_item_money：collect_money_item：inc [MONEY_TOTAL]（下一条 inc [MONEY]）；沿卡链表调 on_item_money(&bonus)，两个全局一起 += bonus"
    },
    "ce_bomb_spent": {
      "addr": "0x4203bc",
      "cavesize": 5,
      "expected": "a1c0f24c00",
      "title": "行为 SDK → BP_ce_bomb_spent：do_bomb 里 consume_bomb(0x4574d0) **刚返回**那一条：mov eax,ds:0x4cf2c0，5 字节绝对寻址无相对量。0x4574d0 全库只有 0x4203b7 一个调用方，所以这里不多不少覆盖每一次炸弹消耗；沿卡链表调 on_bomb_spent()"
    },
    "ce_enemy_drop": {
      "addr": "0x430510",
      "cavesize": 6,
      "expected": "558bec83ec20",
      "title": "行为 SDK → BP_ce_enemy_drop：Enemy__drop_items_and_notify_cards 入口（thiscall，ecx = 敌人）：push ebp/mov ebp,esp/sub esp,0x20，无相对寻址。在引擎撒道具**之前**沿卡链表调 on_enemy_drop_pre(counts)，counts = 敌人 +0x04 起的 20 个 int32（type 1..0x13）"
    },
    "ce_shop_bought": {
      "addr": "0x4183ea",
      "cavesize": 12,
      "expected": "6a066a006a068d8f28020000",
      "title": "商店走两遍 → BP_ce_shop_bought：AbilityShop__on_tick 成交分支（状态已置 5）：push 6 / push 0 / push 6 / lea ecx,[edi+0x228]，无相对寻址；只记「本次进店成交」"
    },
    "ce_shop_reopen": {
      "addr": "0x443b05",
      "cavesize": 5,
      "expected": "a900000200",
      "title": "商店走两遍 → BP_ce_shop_reopen：GameThread__on_tick：test eax,0x20000（eax = GameThread+0xb0，esi = this）；店刚关且成交过且有名额 → eax |= 0x20000 再开一家"
    },
    "ce_snd_gate": {
      "addr": "0x476410",
      "cavesize": 5,
      "expected": "558bec6aff",
      "title": "音效表门：SoundManager::init 入口 → BP_ce_snd_gate（填语音 blob 与新行配置 + I1/I2 自检，然后放行让引擎建 buffer）"
    }
  }
}
