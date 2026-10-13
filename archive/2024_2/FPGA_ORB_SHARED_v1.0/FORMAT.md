# G1/B1和G2中间结果格式 v1.0

所有多字节整数为小端，文件无C/C++结构padding。输入 `input.pgm` 是P5，8位灰度，
maxval=255。前端算法只读PGM像素，不依赖OpenCV。文件头ASCII后图像紧密排列，
y外循环、x内循环。原始源PNG和转换后PGM各有SHA256。

## B1 v1特征包（ORB-SLAM3注入）

文件长度严格64+64*count，无尾字节。magic=8字节 `ORBFEAT\0`。

| 头偏移 | 长度 | 字段/类型 |
|---:|---:|---|
|0|8|magic|
|8|2|u16 version=1|
|10|2|u16 header_bytes=64|
|12|2|u16 record_bytes=64|
|14|1|u8 role：0正常，1初始化|
|15|1|u8 flags=0|
|16|8|u64 timestamp_ns，直接绑定输入整数纳秒|
|24|4|u32 width|
|28|4|u32 height|
|32|4|u32 point_count|
|36|4|u32 nLevels|
|40|4|IEEE754 f32 scaleFactor，整数有理数最近偶数编码|
|44|4|u32 extractor_nFeatures，本次请求预算；初始化5000|
|48|4|u32 descriptor_bytes=32|
|52|4|u32 coordinate_space=0，第0层未去畸变坐标|
|56|4|i32 monoLeft，重叠区之外的点数|
|60|4|u32 reserved=0|

| 每点偏移 | 长度 | 字段/类型 |
|---:|---:|---|
|0|4|f32 x：层整数坐标乘Q24尺度，以有理数最近偶数编码|
|4|4|f32 y，同上|
|8|4|i32 octave|
|12|4|f32 angle：32档中心；11.25°的整数倍|
|16|4|f32 size：floor(31*层Q24尺度)，再精确编码|
|20|4|f32 response：FAST整数0…254的精确编码|
|24|4|i32 class_id=−1|
|28|4|u32 reserved=0|
|32|32|BRIEF256字节，bit i位于byte i/8的bit i%8|

文件内f32不代表前端使用浮点；编码器全程整数除法/位操作，详见binary32_bits。
SLAM沿用B1 FeatureIO的严格解码器，YAML层数/尺度/预算必须一致；同一输入两档
目录 `normal/<timestamp_ns>.orb`、`initialization/<timestamp_ns>.orb`。
数据集中时间戳既可为EuRoC大整数也可为Tello准备集整数，禁止回算浮点秒。
当前G2目录每向量/配置用 `packet.orb`，批量SLAM库使用上述时间戳命名。

## 每层中间结果

每配置的 `trace/trace.json` 含schema=2、图像尺寸、Q24尺度、预算、候选/选中数、
格子边长/行列/空格、mono_left、最终点数。`L<level>_...` 从0层起编号。

| 文件 | 内容 | 字节/行顺序 |
|---|---|---|
|`Lk_pyramid.u8`|未模糊层图|Wk*Hk字节，行优先，无头|
|`Lk_fast.u8`|全层FAST response图，含未过阈值分数|同上，外圈3像素0；候选NMS使用单元范围|
|`Lk_fast_high.u8`|同一FAST分数图保留response>=20，其余0|同上；扫描单元与有效区域筛选前的门控图|
|`Lk_fast_low.u8`|同一FAST分数图保留response>=7，其余0|同上；含高分点，不是仅弱点|
|`Lk_nms.csv`|单元内NMS候选|ASCII CSV：index,x_effective,y_effective,response,tile；单元row/col再y/x|
|`Lk_nms_high.csv`|nms候选中response>=20|同CSV字段；原序保持、重新编号|
|`Lk_nms_low.csv`|nms候选中7<=response<20，仅弱点|同CSV字段；不重复高点，原序保持、重新编号|
|`Lk_grid.csv`|格子选点列表|同CSV字段；坐标相对有效原点(16,16)，输出次序按规范|
|`Lk_grid_thresholds.csv`|选中点阈值来源|index,x_effective,y_effective,response,threshold_used；20为高分池，7为回退池，与grid同序|
|`Lk_moments.csv`|m10/m01和量化角|ASCII CSV：index,x_layer,y_layer,m10,m01,angle_q16,bin；顺序与grid绑定|
|`Lk_gaussian.u8`|描述子使用的模糊层图|Wk*Hk字节，行优先，无头|
|`Lk_descriptors.u8`|选中点描述子|count*32字节；point外循环、byte0…31内循环，与moments相同顺序|
|`output_order.csv`|B1记录顺序映射|record,level,x_layer,y_layer；记录索引从0起|

CSV第一行为字段名，随后十进制整数；m10/m01可带负号。空表只有header，没有虚构
零点。二进制图像/描述子是u8，端序不适用；复位/AXI握手不是此文件协议的一部分。
RTL仿真导出该格式或先显式转换到该格式后比较；禁止在比对工具中隐含排序/容差。

## 比对入口

```bash
python scripts/compare.py --expected expected/trace --actual rtl/trace --level 2 --stage nms
python scripts/compare.py --expected expected/trace --actual rtl/trace --level 0 --stage all
python scripts/compare.py --expected expected/packet.orb --actual rtl/packet.orb --stage packet
python scripts/compare.py --expected expected/L0_fast.u8 --actual rtl/L0_fast.u8 --width 960
```

像素首次差异报告offset,y,x；CSV报告row/字段/期望值/实际值；描述子报告record/byte/bit；
B1报告header字段或record字段/bit。exit0一致、exit1差异、exit2输入缺失/格式错误。
`--stage all`在schema=2检查指定层的12项，历史schema=1仍检查七项。多个层应逐层调用，
不把“某一层通过”表述为全帧通过；完整包重放另检查output_order和B1 packet。
trace.json包含threshold_high/low、dual_threshold、grid_top_k、fixed_fast/grid/blur配置。
B1包版本仍为1；trace schema=2不改变B1布局或reserved，不能把软件观测文件当未确认的AXI协议。
全部向量和中间结果列入SHA256SUMS，按相对路径排序；各向量也有自己的SHA256SUMS。
元数据提供原图路径、源SHA256、准确时间戳、理由和内参标注；顶层清单不哈希自己。
