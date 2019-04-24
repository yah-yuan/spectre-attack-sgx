可控变量:
+ 访问时间的极限,小于该值说明cache命中 
cache_hit_threshold;    default 80
+ 访问次数,相乘是总调用次数
try_times;              default 5
train_rounds;           default 5
+ 每轮训练次数,>1,意味着每训练(train_per_rounds-1)次执行一次投机执行
train_per_rounds;       default 6
+ 密钥长度
secret_len;         default 40

另外,每次访问的数据块大小也是要考虑的,最大为寄存器大小
block_size;             default 1bit 2bit 4bit 8bit
应该阶梯测试访问的速度,理论上8bit8倍于1bit的速度