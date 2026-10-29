#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/** 이펙트에 쓰는 도형. Cube / Sphere는 엔진 기본 도형, 나머지는 설정 에셋이 없으면 코드로 만든다. */
enum class ECRFxShape : uint8
{
	Cube,
	Sphere,
	Spike,   // 납작한 삼각형 (YZ 평면, 밑변 중심 원점, 끝 +Z 100). X축을 카메라로 돌려 쓴다
	Star4,   // 4각 별 (YZ 평면, 반지름 50)
	Star8,   // 8각 별 (YZ 평면, 반지름 50)
	Pyramid, // 사각뿔 (바닥 반지름 50, 높이 100, 피벗은 바닥). 면마다 노멀이 따로라 툰 명암이 면 단위로 갈린다
	Donut,   // 도넛 (바깥 지름 100, 튜브 반지름 15, XY 평면)
	Count
};

namespace CRShapeMeshes
{
	/** 설정(UCRGameSettings Fx Shapes) 에셋 > 코드로 만든 메시. 한 번 만들면 계속 재사용한다. */
	CRASHRUMBLE_API UStaticMesh* Get(ECRFxShape Shape);
}
