#include "SLAUtility.h"
#include "SLAInternal.h"

#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <float.h>

/* 連立１次方程式ソルバー */
struct SLALESolver {
  uint32_t  max_dim;        /* 最大次元数 */
  double*   row_scale;      /* 各行要素のスケール（1/行最大値） */
  uint32_t* change_index;   /* 入れ替えインデックス */
  double*   x_vec;          /* 解ベクトル   */
  double*   err_vec;        /* 誤差ベクトル */
  double**  A_lu;           /* LU分解した係数行列 */
};

/* CRC16(IBM:多項式0x8005を反転した0xa001によるもの) の計算用テーブル */
static const uint16_t CRC16_IBM_BYTE_TABLE[0x100] = { 
	0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
	0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
	0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
	0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
	0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
	0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
	0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
	0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
	0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
	0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
	0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
	0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
	0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
	0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
	0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
	0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
	0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};

/* NLZ計算のためのテーブル */
#define UNUSED 99
static const uint32_t nlz10_table[64] = {
      32,     20,     19, UNUSED, UNUSED,     18, UNUSED,      7,
      10,     17, UNUSED, UNUSED,     14, UNUSED,      6, UNUSED,
  UNUSED,      9, UNUSED,     16, UNUSED, UNUSED,      1,     26,
  UNUSED,     13, UNUSED, UNUSED,     24,      5, UNUSED, UNUSED,
  UNUSED,     21, UNUSED,      8,     11, UNUSED,     15, UNUSED,
  UNUSED, UNUSED, UNUSED,      2,     27,      0,     25, UNUSED,
      22, UNUSED,     12, UNUSED, UNUSED,      3,     28, UNUSED,
      23, UNUSED,      4,     29, UNUSED, UNUSED,     30,     31
};
#undef UNUSED

/* 窓の適用 */
void SLAUtility_ApplyWindow(const double* window, double* data, uint32_t num_samples)
{
  uint32_t smpl;

  SLA_Assert(window != NULL && data != NULL);

  for (smpl = 0; smpl < num_samples; smpl++) {
    data[smpl] *= window[smpl];
  }
}

/* ハン窓を作成 */
void SLAUtility_MakeHannWindow(double* window, uint32_t window_size)
{
  uint32_t  smpl;
  double    x;

  SLA_Assert(window != NULL);

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    window[smpl] = 0.5f - 0.5f * cos(2.0f * SLA_PI * x);
  }
}

/* ブラックマン窓を作成 */
void SLAUtility_MakeBlackmanWindow(double* window, uint32_t window_size)
{
  uint32_t  smpl;
  double    x;

  SLA_Assert(window != NULL);

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    window[smpl] = 0.42f - 0.5f * cos(2.0f * SLA_PI * x) + 0.08f * cos(4.0f * SLA_PI * x);
  }
}

/* サイン窓を作成 */
void SLAUtility_MakeSinWindow(double* window, uint32_t window_size)
{
  uint32_t  smpl;
  double    x;

  SLA_Assert(window != NULL);

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    window[smpl] = sin(SLA_PI * x);
  }
}

/* Vorbis窓を作成 */
void SLAUtility_MakeVorbisWindow(double* window, uint32_t window_size)
{
  uint32_t  smpl;
  double    x;

  SLA_Assert(window != NULL);

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    window[smpl] = sin((SLA_PI / 2.0f) * sin(SLA_PI * x) * sin(SLA_PI * x));
  }
}

/* Tukey窓を作成 */
void SLAUtility_MakeTukeyWindow(double* window, uint32_t window_size, double alpha)
{
  uint32_t  smpl;
  double    x;

  SLA_Assert(window != NULL);

  for (smpl = 0; smpl < window_size; smpl++) {
    x = (double)smpl / (window_size - 1);
    if (x < alpha / 2) {
      window[smpl] = 0.5f * (1.0f + cos(SLA_PI * ((2.0f / alpha) * x - 1)));
    } else if (x > (1 - alpha / 2)) {
      window[smpl] = 0.5f * (1.0f + cos(SLA_PI * ((2.0f / alpha) * x - (2.0f / alpha) + 1)));
    } else {
      window[smpl] = 1.0f;
    }
  }

}

/* FFTのサブルーチン */
/* ftp://ftp.cpc.ncep.noaa.gov/wd51we/random_phase/four1.c から引用 */
static void four1(double data[], unsigned long nn, int isign)
{
	unsigned long n,mmax,m,j,istep,i;
	double wtemp,wr,wpr,wpi,wi,theta;
	double tempr,tempi;

#define SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr

	n=nn << 1;
	j=1;
	for (i=1;i<n;i+=2) {
		if (j > i) {
			SWAP(data[j],data[i]);
			SWAP(data[j+1],data[i+1]);
		}
		m=n >> 1;
		while (m >= 2 && j > m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	mmax=2;
	while (n > mmax) {
		istep=mmax << 1;
		theta=isign*(6.28318530717959/(double)mmax);
		wtemp=sin(0.5*theta);
		wpr = -2.0*wtemp*wtemp;
		wpi=sin(theta);
		wr=1.0;
		wi=0.0;
		for (m=1;m<mmax;m+=2) {
			for (i=m;i<=n;i+=istep) {
				j=i+mmax;
				tempr=wr*data[j]-wi*data[j+1];
				tempi=wr*data[j+1]+wi*data[j];
				data[j]=data[i]-tempr;
				data[j+1]=data[i+1]-tempi;
				data[i] += tempr;
				data[i+1] += tempi;
			}
			wr=(wtemp=wr)*wpr-wi*wpi+wr;
			wi=wi*wpr+wtemp*wpi+wi;
		}
		mmax=istep;
	}
#undef SWAP
}

/* in-placeなFFTルーチン */
/* ftp://ftp.cpc.ncep.noaa.gov/wd51we/random_phase/realft.c から引用・改変 */
static void realft(double data[], unsigned long n, int isign)
{
	unsigned long i,i1,i2,i3,i4,np3;
	double c1=0.5,c2,h1r,h1i,h2r,h2i;
	double wr,wi,wpr,wpi,wtemp,theta;

	theta=3.141592653589793/(double) (n>>1);
	if (isign == 1) {
		c2 = -0.5;
		four1(data,n>>1,1);
	} else {
		c2=0.5;
		theta = -theta;
	}
	wtemp=sin(0.5*theta);
	wpr = -2.0*wtemp*wtemp;
	wpi=sin(theta);
	wr=1.0+wpr;
	wi=wpi;
	np3=n+3;
	for (i=2;i<=(n>>2);i++) {
		i4=1+(i3=np3-(i2=1+(i1=i+i-1)));
		h1r=c1*(data[i1]+data[i3]);
		h1i=c1*(data[i2]-data[i4]);
		h2r = -c2*(data[i2]+data[i4]);
		h2i=c2*(data[i1]-data[i3]);
		data[i1]=h1r+wr*h2r-wi*h2i;
		data[i2]=h1i+wr*h2i+wi*h2r;
		data[i3]=h1r-wr*h2r+wi*h2i;
		data[i4] = -h1i+wr*h2i+wi*h2r;
		wr=(wtemp=wr)*wpr-wi*wpi+wr;
		wi=wi*wpr+wtemp*wpi+wi;
	}
	if (isign == 1) {
		data[1] = (h1r=data[1])+data[2];
		data[2] = h1r-data[2];
	} else {
		data[1]=c1*((h1r=data[1])+data[2]);
		data[2]=c1*(h1r-data[2]);
		four1(data,n>>1,-1);
	}
}

/* FFTハンドル: realftのインデックスのズレを補正 */
void SLAUtility_FFT(double* data, uint32_t n, int32_t sign)
{
  SLA_Assert(data != NULL);
  realft(&data[-1], n, sign);
}

/* CRC16(IBM)の計算 */
uint16_t SLAUtility_CalculateCRC16(const uint8_t* data, uint64_t data_size)
{
  uint16_t crc16;

  /* 引数チェック */
  SLA_Assert(data != NULL);

  /* 初期値 */
  crc16 = 0x0000;

  /* modulo2計算 */
  while (data_size--) {
    /* 補足）多項式は反転済みなので、この計算により入出力反転済みとできる */
    crc16 = (crc16 >> 8) ^ CRC16_IBM_BYTE_TABLE[(crc16 ^ (*data++)) & 0xFF];
  }

  return crc16;
}

/* NLZ（最上位ビットから1に当たるまでのビット数）を計算する黒魔術 */
/* ハッカーのたのしみ参照 */
static uint32_t nlz10(uint32_t x)
{
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x & ~(x >> 16);
  x = (x << 9) - x;
  x = (x << 11) - x;
  x = (x << 14) - x;
  return nlz10_table[x >> 26];
}

/* ceil(log2(val)) を計算する */
uint32_t SLAUtility_Log2Ceil(uint32_t val)
{
  SLA_Assert(val != 0);
  return 32U - nlz10(val - 1);
}

/* 2の冪乗数に切り上げる ハッカーのたのしみ参照 */
uint32_t SLAUtility_RoundUp2Powered(uint32_t val)
{
  val--;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  return val + 1;
}

/* LR -> MS（double） */
void SLAUtility_LRtoMSDouble(double **data,
    uint32_t num_channels, uint32_t num_samples)
{
  uint32_t  smpl;
  double    mid, side;

  SLA_Assert(data != NULL);
  SLA_Assert(data[0] != NULL);
  SLA_Assert(data[1] != NULL);
  SLA_Assert(num_channels >= 2);
  SLAUTILITY_UNUSED_ARGUMENT(num_channels);

  for (smpl = 0; smpl < num_samples; smpl++) {
    mid   = (data[0][smpl] + data[1][smpl]) / 2;
    side  = data[0][smpl] - data[1][smpl];
    data[0][smpl] = mid; 
    data[1][smpl] = side;
  }
}

/* LR -> MS（int32_t） */
void SLAUtility_LRtoMSInt32(int32_t **data, 
    uint32_t num_channels, uint32_t num_samples)
{
  uint32_t  smpl;
  int32_t   mid, side;

  SLA_Assert(data != NULL);
  SLA_Assert(data[0] != NULL);
  SLA_Assert(data[1] != NULL);
  SLA_Assert(num_channels >= 2);
  SLAUTILITY_UNUSED_ARGUMENT(num_channels); 

  for (smpl = 0; smpl < num_samples; smpl++) {
    mid   = (data[0][smpl] + data[1][smpl]) >> 1; /* 注意: 右シフト必須(/2ではだめ。0方向に丸められる) */
    side  = data[0][smpl] - data[1][smpl];
    /* 戻るかその場で確認 */
    SLA_Assert(data[0][smpl] == ((((mid << 1) | (side & 1)) + side) >> 1));
    SLA_Assert(data[1][smpl] == ((((mid << 1) | (side & 1)) - side) >> 1));
    data[0][smpl] = mid; 
    data[1][smpl] = side;
  }
}

/* MS -> LR（int32_t） */
void SLAUtility_MStoLRInt32(int32_t **data, 
    uint32_t num_channels, uint32_t num_samples)
{
  uint32_t  smpl;
  int32_t   mid, side;

  SLA_Assert(data != NULL);
  SLA_Assert(data[0] != NULL);
  SLA_Assert(data[1] != NULL);
  SLA_Assert(num_channels >= 2);
  SLAUTILITY_UNUSED_ARGUMENT(num_channels);

  for (smpl = 0; smpl < num_samples; smpl++) {
    side  = data[1][smpl];
    mid   = (data[0][smpl] << 1) | (side & 1);
    data[0][smpl] = (mid + side) >> 1;
    data[1][smpl] = (mid - side) >> 1;
  }
}

/* round関数（C89で定義されてない） */
double SLAUtility_Round(double d)
{
    return (d >= 0.0f) ? floor(d + 0.5f) : -floor(-d + 0.5f);
}

/* log2関数（C89で定義されていない） */
double SLAUtility_Log2(double x)
{
#define INV_LOGE2 (1.4426950408889634)  /* 1 / log(2) */
  return log(x) * INV_LOGE2;
#undef INV_LOGE2
}

/* プリエンファシス(double) */
void SLAUtility_PreEmphasisDouble(double* data, uint32_t num_samples, int32_t coef_shift)
{
  uint32_t  smpl;
  double    prev, tmp;
  double    coef;

  SLA_Assert(data != NULL);

  /* フィルタ係数の計算 */
  coef = (pow(2, coef_shift) - 1.0f) * pow(2, -coef_shift);

  /* フィルタ適用 */
  prev = 0.0f;
  for (smpl = 0; smpl < num_samples; smpl++) {
    tmp         = data[smpl];
    data[smpl] -= prev * coef;
    prev        = tmp;
  }

}

/* プリエンファシス(int32) */
void SLAUtility_PreEmphasisInt32(int32_t* data, uint32_t num_samples, int32_t coef_shift)
{
  uint32_t  smpl;
  int32_t   prev_int32, tmp_int32;
  const int32_t coef_numer = (1 << coef_shift) - 1;

  SLA_Assert(data != NULL);

  /* フィルタ適用 */
  prev_int32 = 0;
  for (smpl = 0; smpl < num_samples; smpl++) {
    tmp_int32   = data[smpl];
    data[smpl] -= (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(prev_int32 * coef_numer, coef_shift);
    prev_int32  = tmp_int32;
  }

}

/* デエンファシス(int32) */
void SLAUtility_DeEmphasisInt32(int32_t* data, uint32_t num_samples, int32_t coef_shift)
{
  uint32_t  smpl;
  const int32_t coef_numer = (1 << coef_shift) - 1;

  SLA_Assert(data != NULL);

  /* フィルタ適用 */
  for (smpl = 1; smpl < num_samples; smpl++) {
    data[smpl] += (int32_t)SLAUTILITY_SHIFT_RIGHT_ARITHMETIC(data[smpl - 1] * coef_numer, coef_shift);
  }

}

/* 連立一次方程式ソルバーの作成 */
struct SLALESolver* SLALESolver_Create(uint32_t max_dim)
{
  uint32_t dim;
  struct SLALESolver* lesolver;

  lesolver     = (struct SLALESolver *)malloc(sizeof(struct SLALESolver));
  lesolver->max_dim      = max_dim;
  lesolver->row_scale    = (double *)malloc(sizeof(double) * max_dim);
  lesolver->change_index = (uint32_t *)malloc(sizeof(uint32_t) * max_dim);
  lesolver->x_vec        = (double *)malloc(sizeof(double) * max_dim);
  lesolver->err_vec      = (double *)malloc(sizeof(double) * max_dim);
  lesolver->A_lu         = (double **)malloc(sizeof(double*) * max_dim);

  for (dim = 0; dim < max_dim; dim++) {
    lesolver->A_lu[dim] = (double *)malloc(sizeof(double) * max_dim);
  }

  return lesolver;
}

/* 連立一次方程式ソルバーの破棄 */
void SLALESolver_Destroy(struct SLALESolver* lesolver)
{
  if (lesolver != NULL) {
    uint32_t dim;
    for (dim = 0; dim < lesolver->max_dim; dim++) {
      NULLCHECK_AND_FREE(lesolver->A_lu[dim]);
    }
    NULLCHECK_AND_FREE(lesolver->A_lu);
    NULLCHECK_AND_FREE(lesolver->row_scale);
    NULLCHECK_AND_FREE(lesolver->change_index);
    NULLCHECK_AND_FREE(lesolver->x_vec);
    NULLCHECK_AND_FREE(lesolver->err_vec);
  }
}

/* LU分解 */
static int32_t SLALESolver_LUDecomposion(
    double* const* A, uint32_t dim, uint32_t *change_index, double *row_scale)
{
  uint32_t row, col, k, max_index;
  double max, denom_elem, sum;

  SLA_Assert(A != NULL);
  SLA_Assert(change_index != NULL);
  SLA_Assert(row_scale != NULL);

  /* 各行のスケール（1/行最大値）を計算 */
  for (row = 0; row < dim; row++) {
    max = 0.0f;
    for (col = 0; col < dim; col++) {
      if (fabs(A[row][col]) > max) {
        max = fabs(A[row][col]);
      }
    }
    if (fabs(max) <= FLT_EPSILON) {
      /* 行列が特異 */
      return -1;
    }
    row_scale[row] = 1.0f / max;
  }

  /* Croutのアルゴリズム */
  for (col = 0; col < dim; col++) {
    /* β要素（L,下三角行列要素）の計算 */
    for (row = 0; row < col; row++) {
      sum = A[row][col];              /* Aのこの要素は1回しか参照されない */
      for (k = 0; k < row; k++) {
        sum -= A[row][k] * A[k][col]; /* この時Aはin-placeでα*βに置き換わっている */
      }
      A[row][col] = sum;
    }

    /* α（U,下三角行列要素）の計算 */
    max       = 0.0f;
    max_index = row;
    for (row = col; row < dim; row++) {
      sum = A[row][col];              /* Aのこの要素は1回しか参照されない */
      for (k = 0; k < col; ++k) {
        sum -= A[row][k] * A[k][col];
      }
      A[row][col] = sum;
      
      /* ピボットを選択 */
      if ((row_scale[row] * fabs(sum)) >= max) {
        max       = (row_scale[row] * fabs(sum));
        max_index = row;
      }
    }

    /* 行交換 
     * 注意）これにより結果のAは求めるべき行列の行を交換したものに変化する。*/
    if (col != max_index) {
      for (k = 0; k < dim; k++) {
        double tmp;
        /* スワップ */
        tmp = A[max_index][k];
        A[max_index][k] = A[col][k];
        A[col][k] = tmp;
      }
      /* スケールの入れ替え */
      row_scale[max_index] = row_scale[col];
    }
    change_index[col] = max_index;

    /* 部分ピボット選択の範囲で特異 
     * 補足）完全にすれば改善される場合がある */
    if (fabs(A[col][col]) <= FLT_EPSILON) {
      /* 行列が特異 */
      return -1;
    }

    /* ピボット要素で割り、α（U,下三角行列要素）を完成させる */
    if (col != (dim - 1)) {
      denom_elem = 1.0f / A[col][col];
      for (row = col + 1; row < dim; row++) {
        A[row][col] *= denom_elem;
      }
    }
  }

  return 0;
}

/* 前進代入と後退代入により求解 */
static void SLALESolver_LUDecomposionForwardBack(
    double const* const* A, double* b, uint32_t dim, const uint32_t* change_index)
{
  uint32_t  row, col, pivod, nonzero_row;
  double    sum;

  SLA_Assert(A != NULL);
  SLA_Assert(b != NULL);
  SLA_Assert(change_index != NULL);

  /* 前進代入 */
  nonzero_row = 0;
  for (row = 0; row < dim; row++) {
    /* 交換を元に戻す */
    pivod     = change_index[row];
    sum       = b[pivod];
    b[pivod]  = b[row];

    if (nonzero_row != 0) {
      /* bの非零要素から積和演算 */
      for (col = nonzero_row; col < row; col++) {
        sum -= A[row][col] * b[col];  /* α*b */
      }
    } else if (sum != 0.0f) {
      nonzero_row = row;
    }

    b[row] = sum;
  }

  /* 後退代入 */
  for (row = dim - 1; row < dim; row--) {
    sum = b[row];
    for (col = row + 1; col < dim; col++) {
      sum -= A[row][col] * b[col];  /* β*b */
    }

    /* 解を代入 */
    b[row] = sum / A[row][row];
    if (row == 0) {
      break;
    }
  }
}

/* LU分解（反復改良付き） */
int32_t SLALESolver_Solve(
    struct SLALESolver* lesolver,
    const double** A, double* b, uint32_t dim, uint32_t itration_count)
{
  uint32_t row, col, count;
  int32_t  ret;
  long double error;  /* 残差はなるべく高精度 */

  SLA_Assert(lesolver != NULL);
  SLA_Assert(A != NULL);
  SLA_Assert(b != NULL);

  /* LU分解用のAを作成 */
  for (row = 0; row < dim; row++) {
    memcpy(lesolver->A_lu[row], A[row], sizeof(double) * dim);
  }

  /* bベクトルのコピー */
  memcpy(lesolver->x_vec, b, sizeof(double) * dim);

  /* 一旦、解xを求める */
  ret = SLALESolver_LUDecomposion(
      lesolver->A_lu, dim, lesolver->change_index, lesolver->row_scale);
  if (ret != 0) {
    return -1;
  }
  SLALESolver_LUDecomposionForwardBack(
      (double const* const*)lesolver->A_lu, lesolver->x_vec, dim, lesolver->change_index);

  /* 反復改良 */
  for (count = 0; count < itration_count; count++) {
    for (row = 0; row < dim; row++) {
      error = -b[row];
      for (col = 0; col < dim; col++) {
        error += A[row][col] * lesolver->x_vec[col];
      }
      lesolver->err_vec[row] = (double)error;
    }

    /* 残差について解く */
    SLALESolver_LUDecomposionForwardBack(
        (double const* const*)lesolver->A_lu, lesolver->err_vec, dim, lesolver->change_index);

    /* 残差を引く */
    for (row = 0; row < dim; row++) {
      lesolver->x_vec[row] -= lesolver->err_vec[row];
    }
  }

  /* 改良した結果をbにコピー */
  memcpy(b, lesolver->x_vec, sizeof(double) * dim);
 
  return 0;
}
