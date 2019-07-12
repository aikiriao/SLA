#ifndef SLA_H_INCLUDED
#define SLA_H_INCLUDED

#include <stdint.h>

/* バージョン文字列 */
#define SLA_VERSION_STRING          "0.0.1(beta)"
/* フォーマットバージョン */
#define SLA_FORMAT_VERSION			    1
/* ヘッダのサイズ */
#define SLA_HEADER_SIZE			        43
/* ブロックヘッダのサイズ */
#define SLA_BLOCK_HEADER_SIZE			  10
/* サンプル数の無効値 */
#define SLA_NUM_SAMPLES_INVALID		  0xFFFFFFFF
/* SLAブロック数の無効値 */
#define SLA_NUM_BLOCKS_INVALID		  0xFFFFFFFF
/* 最大ブロックサイズの無効値 */
#define SLA_MAX_BLOCK_SIZE_INVAILD	0xFFFFFFFF

/* ブロックエンコード/デコードに十分なブロックサイズ */
#define SLA_CalculateSufficientBlockSize(num_channels, num_samples, bit_per_sample)	\
	(2 * (num_channels) * (num_samples) * ((bit_per_sample) / 8))

/* API結果型 */
typedef enum SLAApiResultTag {
	SLA_APIRESULT_OK = 0,
	SLA_APIRESULT_NG,
  SLA_APIRESULT_INVALID_ARGUMENT,           /* 無効な引数 */
  SLA_APIRESULT_EXCEED_HANDLE_CAPACITY,     /* ハンドルの許容範囲外 */
  SLA_APIRESULT_INSUFFICIENT_BUFFER_SIZE,   /* バッファサイズが不足 */
  SLA_APIRESULT_INVAILD_CHPROCESSMETHOD,    /* チャンネル処理を行えないチャンネル数が指定された */
  SLA_APIRESULT_FAILED_TO_CALCULATE_COEF,   /* 予測係数を求めるのに失敗した */
  SLA_APIRESULT_FAILED_TO_PREDICT,          /* 予測に失敗した */
  SLA_APIRESULT_FAILED_TO_SYNTHESIZE,       /* 合成に失敗した */
  SLA_APIRESULT_INSUFFICIENT_DATA_SIZE,     /* データ不足 */
  SLA_APIRESULT_INVALID_HEADER_FORMAT,      /* ヘッダが不正 */
  SLA_APIRESULT_DETECT_DATA_CORRUPTION,     /* データ破壊を検出した */
  SLA_APIRESULT_FAILED_TO_FIND_SYNC_CODE,   /* 同期コードを発見できなかった */
  SLA_APIRESULT_INVALID_WINDOWFUNCTION_TYPE, /* 不正な窓関数が指定された */
  SLA_APIRESULT_NO_DATA_FRAGMENTS             /* 回収可能なデータ片が存在しない */
} SLAApiResult;

/* マルチチャンネル処理方法 */
typedef enum SLAChannelProcessMethodTag {
	SLA_CHPROCESSMETHOD_NONE = 0,	          /* 何もしない */
	SLA_CHPROCESSMETHOD_STEREO_MS	          /* ステレオMS処理 */
} SLAChannelProcessMethod;

/* 窓関数タイプ */
typedef enum SLAWindowFunctionTypeTag {
	SLA_WINDOWFUNCTIONTYPE_RECTANGULAR = 0,	  /* 矩形窓（何もしない） */
	SLA_WINDOWFUNCTIONTYPE_SIN,	              /* サイン窓             */
	SLA_WINDOWFUNCTIONTYPE_HANN,	            /* ハン窓               */
	SLA_WINDOWFUNCTIONTYPE_BLACKMAN,	        /* ブラックマン窓       */
	SLA_WINDOWFUNCTIONTYPE_VORBIS	            /* Vorbis窓             */
} SLAWindowFunctionType;

/* 波形フォーマット */
struct SLAWaveFormat {
	uint32_t  num_channels;			/* チャンネル数             */
	uint32_t  bit_per_sample;		/* サンプルあたりビット数   */
	uint32_t  sampling_rate;		/* サンプリングレート       */
  uint8_t   offset_lshift;    /* オフセット分の左シフト量 */
};

/* エンコードパラメータ */
struct SLAEncodeParameter {
	uint32_t                parcor_order;			        /* PARCOR係数次数 */
	uint32_t                longterm_order;		        /* ロングターム次数 */
	uint32_t                lms_order_par_filter;	    /* LMS1フィルタあたりの次数 */
	SLAChannelProcessMethod	ch_process_method;	      /* マルチチャンネル処理法 */
  SLAWindowFunctionType   window_function_type;     /* 窓関数の種類 */
	uint32_t                max_num_block_samples;    /* ブロックあたりサンプル数 */
};

/* SLAヘッダ情報 */
struct SLAHeaderInfo {
	struct SLAWaveFormat      wave_format;	      /* 波形フォーマット         */
  struct SLAEncodeParameter encode_param;       /* エンコードパラメータ     */
	uint32_t                  num_samples;			  /* 全サンプル数             */
  uint32_t                  num_blocks;         /* ブロック数               */
	uint32_t                  max_block_size;		  /* 最大ブロックサイズ[byte] */
  uint32_t                  max_bit_per_second; /* 最大bps                  */
};

#endif /* SLA_H_INCLUDED */
