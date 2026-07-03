# Coding Conventions

> Based on Epic C++ Coding Standard for Unreal Engine
> Reference: https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine

## 1. Class Organization

- 읽는 사람을 염두에 두고 조직한다
- public 구현을 먼저 선언하고, private 구현이 뒤따른다

## 2. Copyright Notice

- 공개 배포용 소스 파일(.h, .cpp, .xaml) 첫 줄에 저작권 고지 포함 필수
- 포맷: `// Copyright Epic Games, Inc. All Rights Reserved.`

## 3. Naming Conventions

### Type Prefixes

| Prefix | Usage |
|--------|-------|
| `T` | 템플릿 클래스 |
| `U` | UObject 상속 클래스 |
| `A` | AActor 상속 클래스 |
| `S` | SWidget 상속 클래스 |
| `I` | 추상 인터페이스 |
| `C` | 에픽의 콘셉트 유사 클래스 타입 |
| `E` | 열거형 (Enum) |
| `b` | 부울 변수 접두사 |
| `F` | 그 외 대부분의 클래스 |

### Typedef Rules

- 구조체의 typedef: `F` 접두사
- UObject의 typedef: `U` 접두사
- 특정 템플릿 인스턴스화의 typedef는 더 이상 템플릿이 아니며 알맞은 접두사를 붙인다
  ```cpp
  typedef TArray<FMytype> FArrayOfMyTypes;
  ```

### Template Parameter Rules

- 타입 카테고리를 알 수 없는 템플릿 파라미터와 중첩 타입 에일리어스는 접두사 규칙 대상이 아님
- 설명적 용어 뒤에 `Type` 접미사 사용 권장
- `In` 접두사로 템플릿 파라미터를 에일리어스와 구분
  ```cpp
  template <typename InElementType>
  class TContainer
  {
  public:
      using ElementType = InElementType;
  };
  ```

### Additional Notes

- C#에서는 접두사가 생략됨
- 언리얼 헤더 툴(UHT)은 대부분 올바른 접두사가 필요하므로 접두사를 제공하는 것이 중요

### General Naming

- PascalCase 사용: 각 단어의 첫 글자 대문자, 단어 사이 언더스코어 없음
- 미국 영어 철자법 및 문법 사용
- 타입 및 변수 이름은 명사
- 메서드 이름은 이펙트를 설명하는 동사 또는 반환 값을 설명하는 동사
- bool 반환 함수는 true/false 질문형: `IsVisible()`, `ShouldClearBuffer()`
- 프로시저(반환 값 없는 함수)는 강한 동사 + 오브젝트. `Handle`, `Process` 등 모호한 동사 금지
- Out 참조 파라미터는 `Out` 접두사: `OutResult`
- In/Out 파라미터가 부울이면 `bOutResult` 형태
- 값 반환 함수는 반환 값을 설명해야 함
- 모든 변수는 자체 줄에서 선언 (JavaDocs 스타일)
- 이름은 명확하고, 확실하고, 내용을 파악할 수 있어야 함. 과도한 약어 금지

### Macro Naming

- 매크로 이름은 모두 대문자, 단어는 언더스코어로 분리, `UE_` 접두사 사용
  ```cpp
  #define UE_AUDIT_SPRITER_IMPORT
  ```

## 4. Inclusive Word Choice

코드의 모든 명명(클래스, 함수, 변수, 파일, 폴더, 플러그인), UI 텍스트, 코멘트, 체인지리스트 설명에 적용한다.

### Prohibited Terms & Replacements

| 금지 용어 | 대체 용어 |
|-----------|----------|
| Blacklist | deny list, block list, exclude list, forbidden list |
| Whitelist | allow list, include list, trust list, safe list, approved list |
| Master | primary, source, controller, template, reference, main, leader |
| Slave | secondary, replica, agent, follower, worker, cluster node |

### Gender Inclusivity

- 가상의 인물은 단수형이라도 they/them/their 사용
- 사람 이외 사물은 it/its로 지칭
- 성별을 상정하는 집합 명사(guys 등) 사용 금지

### General

- 속어 및 관용어구 사용 금지
- 신성 모독적 표현 금지
- 이중적 의미가 있는 기술 용어(abort, execute, native 등)는 맥락에서 적절한지 검토

## 5. Portable C++ Types

| Type | Size | Usage |
|------|------|-------|
| `bool` | - | 불리언 (크기 추정 금지, `BOOL`은 컴파일 안 됨) |
| `TCHAR` | - | 문자 (크기 추정 금지) |
| `uint8` / `int8` | 1 byte | 부호 없는/있는 바이트 |
| `uint16` / `int16` | 2 bytes | 부호 없는/있는 short |
| `uint32` / `int32` | 4 bytes | 부호 없는/있는 정수 |
| `uint64` / `int64` | 8 bytes | 부호 없는/있는 quad word |
| `float` | 4 bytes | 단정밀도 부동소수점 |
| `double` | 8 bytes | 배정밀도 부동소수점 |
| `PTRINT` | - | 포인터 크기 정수 (크기 추정 금지) |

- `int`/`unsigned int`는 정수 너비가 중요치 않은 경우 사용 가능 (최소 32비트 보장)
- 시리얼라이즈/리플리케이트 포맷에서는 반드시 명시적 크기 타입 사용

## 6. Standard Library Usage

표준 라이브러리와 UE 자체 라이브러리 중 더 나은 결과를 제공하는 옵션을 사용한다. 동일 API에서 UE와 표준 라이브러리 언어를 혼합 사용하지 않는다.

| Library | Policy |
|---------|--------|
| `<atomic>` | 새 코드에서 사용해야 함. UE의 `TAtomic`은 부분적 구현이며 유지보수 예정 없음 |
| `<type_traits>` | 레거시 UE 특성과 겹칠 때 사용. 새 특성은 소문자 `value`/`type`으로 작성 |
| `<initializer_list>` | 중괄호 이니셜라이저 문법 지원에 사용 |
| `<regex>` | 에디터 전용 코드 내에 캡슐화하여 사용 가능 |
| `<limits>` | `std::numeric_limits` 온전히 사용 가능 |
| `<cmath>` | 모든 부동소수점 함수 사용 가능 |
| `<cstring>` | `memcpy()`/`memset()`은 퍼포먼스 이점이 있을 경우 `FMemory` 대신 사용 가능 |

- 표준 컨테이너와 스트링은 interop 코드를 제외하고 사용 금지

## 7. Comments

### Guidelines

- 코드 자체만으로 뜻을 알 수 있도록 코드를 작성
- 코멘트는 의도를 설명 (코드는 구현을 설명)
- 나쁜 코드에 긴 코멘트 대신 코드를 재작성
- 코드와 모순되는 코멘트 금지
- 코드 한 줄의 의도를 바꾸더라도 반드시 코멘트 업데이트

### JavaDoc Format

- 클래스 코멘트: 이 클래스가 해결하는 문제, 생성 이유
- 메서드 코멘트: 퍼블릭 선언 위치에 한 번만 포함
  - 함수의 목적
  - 파라미터: 측정 단위, 예상 값 범위, 불가능한 값, 상태/오류 코드 의미
  - 반환값: 함수 목적에 명시화된 경우 명시적 `@return` 불필요
  - `@warning`, `@note`, `@see`, `@deprecated`는 별도 줄에 선언
- `@param` 스타일(여러 줄) 또는 설명에 통합(단순 함수)
- 메서드 구현 세부사항이나 호출자와 무관한 오버라이드 정보는 선언이 아닌 구현 안에 코멘트

## 8. Const Correctness

- 함수 실행인자가 수정되지 않으면 const 포인터 또는 참조로 전달
- 오브젝트를 수정하지 않는 메서드는 const 지정
- 컨테이너 수정 없이 반복작업 시 const 사용
- by-value 파라미터와 로컬에서도 const 선호 (가독성 향상)
- 예외: 컨테이너 안으로 이동하는 pass-by-value 파라미터
  ```cpp
  void FBlah::SetMemberArray(TArray<FString> InNewArray)
  {
      MemberArray = MoveTemp(InNewArray);
  }
  ```
- 포인터 자체를 const로 만들 때는 끝에 const 키워드: `T* const Ptr`
- 반환 타입에는 const 사용 금지 (이동 시맨틱 제한, 기본 타입 컴파일 경고)
  - 포인터의 타깃 타입이나 반환 레퍼런스에는 적용 안 됨

## 9. Modern C++ Features

UE는 기본 C++20 언어 버전으로 컴파일하며, 최소 요구 버전은 C++20이다.

### static_assert

- 컴파일 시간 어서트가 필요한 경우 사용

### override and final

- 사용을 강력히 권장
- 파생 클래스에서 가상 함수 오버라이드 시 `virtual`과 `override` 둘 다 사용

### nullptr

- 모든 경우 C 스타일 `NULL` 매크로 대신 사용
- C++/CX 빌드에서는 `TYPE_OF_NULLPTR` 매크로 사용

### auto

- **기본적으로 사용 금지**. 초기화 타입을 항상 명시
- C++20 구조체 바인딩도 사용 금지 (variadic auto)
- auto 사용 가능 경우:
  - 변수에 람다를 바인딩할 때 (타입을 코드로 표현 불가)
  - 이터레이터 타입이 매우 장황하여 가독성에 악영향일 때
  - 템플릿 코드에서 표현식 타입을 쉽게 식별 불가할 때
- auto 사용 시 `const`, `&`, `*`를 정확히 사용

### Range-Based For

- 가독성과 유지보수성 향상에 도움이 되므로 사용 추천
- TMap 이주 시 `Key()`/`Value()` → `Kvp.Key`/`Kvp.Value`

### Lambda

- 자유롭게 사용 가능. 최적 길이는 두 구문 정도
- 자동 캡처(`[&]`, `[=]`) 대신 **명시적 캡처** 사용
  - 지연 실행 시 포인터 참조 캡처가 허상 참조 유발 가능
  - `[=]`는 멤버 변수 참조 시 묵시적으로 `this` 캡처
  - 잘못 캡처된 UObject 포인터는 가비지 컬렉터에 보이지 않음
- 대규모 람다이거나 다른 함수 호출 결과 반환 시 명시적 반환 타입 사용
- 사소하지 않은 람다는 일반 함수처럼 문서화
- 스테이트풀 람다는 함수 포인터에 할당 불가

### Strongly-Typed Enum

- `enum class`로 기존 네임스페이스 열거형 대체
- 블루프린트 노출 열거형은 `uint8` 기반 유지
- 플래그용 Enum은 `ENUM_CLASS_FLAGS(EnumType)` 매크로 사용
- truth 컨텍스트 비교는 `None` 열거형 사용: `(Flags & EFlags::Flag1) != EFlags::None`
- `UPROPERTY()`로 지원되며 기존 `TEnumAsByte<>` 우회법을 대체

### Move Semantics

- `TArray`, `TMap`, `TSet`, `FString` 등 주요 컨테이너에 move 컨스트럭터/할당 연산자 있음
- 명시적 호출 시 `MoveTemp` 사용 (`std::move`의 UE 버전)
- 값으로 컨테이너/스트링 반환은 임시 복사 비용 없이 표현성에 유용

### Default Member Initializer

- 클래스 내에서 디폴트값 정의 가능
- 장점: 여러 컨스트럭터에 걸친 이니셜라이저 복제 불필요, 초기화/선언 순서 혼동 방지
- 단점: 디폴트값 변경 시 모든 종속 파일 리빌드 필요
- 초기화 불가능 항목: 베이스 클래스, UObject 서브오브젝트, forward-declared 타입 포인터, 컨스트럭터 아규먼트에서 추론한 값, 여러 단계에 걸쳐 초기화된 멤버
- 게임 코드에 더 적합, 엔진 코드에서는 판단 필요
- 디폴트값에 환경설정 파일 사용을 고려

## 10. Third Party Code

- 엔진 라이브러리 코드 수정 시 `//@UE5` 코멘트 + 변경 이유 태그 필수
- 서드 파티 코드는 검색 가능한 포맷으로 표시:
  ```cpp
  // @third party code - BEGIN PhysX
  #include <physx.h>
  // @third party code - END PhysX
  ```

## 11. Code Formatting

### Braces

- 새 줄에 중괄호 배치
- 단일 구문 블록에도 항상 중괄호 포함

### If-Else

- 각 실행 블록은 반드시 중괄호로 묶기
- 여러 갈래 if에서 각 `else if`는 첫 번째 `if`와 같은 들여쓰기

### Tabs & Indentation

- 실행 블록별 코드 들여쓰기
- 줄 시작 공백에는 스페이스가 아니라 **탭** 사용 (탭 크기 4자)
- 탭 이외 문자에 코드 줄을 맞추기 위해 스페이스 사용 가능
- C#에서도 탭 사용

### Switch

- 빈 케이스 제외, 다음 케이스로 넘어감을 명시적으로 표시 (break, return, 또는 `// falls through`)
- 디폴트 케이스는 항상 만들어 두고 break 포함

## 12. Namespaces

- 해당 시 클래스, 함수, 변수 구성에 네임스페이스 사용
- `UCLASS`, `USTRUCT` 등 정의 시 네임스페이스 사용 불가 (UnrealHeaderTool 미지원)
- 새 API는 `UE::` 네임스페이스에 배치, 이상적으로 중첩 네임스페이스 사용 (예: `UE::Audio::`)
- 구현 세부 정보는 `Private` 네임스페이스 (예: `UE::Audio::Private::`)
- 전역 범위에 `.cpp` 파일에서도 `using` 선언 금지 (unity 빌드 문제)
- 다른 네임스페이스/함수 바디 안에서는 `using` 선언 가능
- 전방 선언 타입은 각 네임스페이스 안에서 선언 (링크 오류 방지)
- `using` 선언으로 특정 변수만 에일리어싱 가능 (`using Foo::FBar`), 단 언리얼 코드에서는 일반적이지 않음
- 매크로는 네임스페이스 불가 → `UE_` 접두사 사용

## 13. Physical Dependencies

- 파일 이름에 접두사 가급적 금지 (`UScene.cpp` → `Scene.cpp`)
- 모든 헤더는 `#pragma once`로 복수 include 방지
- 헤더 include 대신 전방 선언 가능한 경우 전방 선언 사용
- 세밀하게 include: `Core.h` 대신 필요한 특정 헤더 include
- 간접 include에 의존하지 않음 — 필요한 것은 전부 직접 include
- 모듈의 Public/Private 소스 디렉터리 구분
- 큰 함수는 논리적 하위 함수로 분할 (컴파일러 최적화, 빌드 시간)
- 인라인 함수 과다 사용 금지: 사소한 접근자 또는 프로파일링 정당화 시에만
- `FORCEINLINE` 사용은 보수적으로

## 14. Encapsulation

- 클래스 멤버는 public/protected 인터페이스 일부가 아니면 거의 항상 private 선언
- 파생 클래스 전용 필드는 private + protected 접근자 제공
- 더 이상 파생시킬 클래스가 아닌 경우 `final` 사용

## 15. General Style Issues

- **종속성 거리 최소화**: 변수 사용 직전에 값 설정
- **메서드 분할**: 큰 메서드보다 이름을 잘 지은 하위 메서드 연속 호출이 더 이해하기 수월
- 함수 이름과 괄호 사이 스페이스 금지
- **컴파일러 경고 수정** 필수 (`#pragma` 억제는 최후 수단)
- 파일 끝에 빈 줄 하나 (gcc 호환)
- 디버그 코드는 잘 다듬어진 상태가 아니면 체크인 금지
- 문자열 리터럴에 항상 `TEXT()` 매크로 사용
- 루프에서 동일 연산 반복 회피 (공통 하위 표현식은 루프 밖으로)
- 핫 리로드 기능 염두: 종속성 최소화, 리로드 중 변할 함수에 인라인/템플릿 금지
- **복잡한 표현식은 중간 변수로 간소화**
- **포인터/레퍼런스 스페이스**는 오른쪽에 한 칸: `FShaderType* Ptr` (O), `FShaderType *Ptr` (X)
- **섀도잉된 변수 금지**
- **익명 리터럴 사용 금지** — 명명된 상수로 의미 설명
- 헤더에 특수한 스태틱 변수 정의 금지 → `extern` + .cpp에서 정의

## 16. API Design Guidelines

- **bool 함수 파라미터 금지** — 열거형 사용 (ENUM_CLASS_FLAGS)
  - 예외: setter처럼 완전한 상태일 때 (`void SetEnabled(bool bEnabled)`)
- **너무 긴 파라미터 목록 금지** — 전용 구조체 전달 고려
- **bool과 FString을 사용한 함수 오버로드 금지** (`TEXT("String")`이 bool 오버로드를 호출함)
- **인터페이스 클래스**는 항상 추상형, `I` 접두사, 멤버 변수 금지
- 오버라이딩 메서드 선언 시 `virtual`과 `override` 둘 다 사용

## 17. Platform-Specific Code

- 플랫폼별 코드는 적합한 이름의 하위 디렉터리에 추상화 및 구현
  ```
  Engine/Platforms/[PLATFORM]/Source/Runtime/Core/Private/[PLATFORM]PlatformMemory.cpp
  ```
- `PLATFORM_[PLATFORM]` 형태 사용 금지 → 하드웨어 추상화 레이어(HAL) 확장
- define이 필요한 경우 플랫폼 프로퍼티를 설명하는 `#define` 사용 (예: `PLATFORM_USE_PTHREADS`)
- `Platform.h`에 디폴트값 설정, 플랫폼별 `Platform.h`에서 오버라이드
- 크로스 플랫폼 코드가 플랫폼별 코드에 종속되어서는 안 됨
- NDA 플랫폼(PlayStation, Xbox, Nintendo Switch) 코드는 플랫폼별 폴더 필수

## 18. String Handling

- 문자열 리터럴에 `TEXT()` 매크로 필수
- `TEXT()` 없으면 `FString` 생성 시 원치 않는 변환 프로세스 유발
