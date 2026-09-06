# 웹 포팅 로드맵 (Web Re-implementation Roadmap)

본 문서는 JKENGINE을 웹 환경에서 동작하도록 **재구현(re-implementation)** 하기 위한 단계별 로드맵과 기술 선택 가이드를 제공합니다.

---

## 1. 포팅 원칙

1. **비즈니스 로직 우선**: DOS/VESA/인터럽트 같은 플랫폼 종속 코드는 제거하고, 데이터 모델과 업무 흐름을 보존합니다.
2. **HTML/CSS 기본 요소 활용**: 버튼, 에디트, 리스트박스 등은 가능한 한 브라우저 기본 폼 요소를 사용합니다.
3. **Canvas는 보조적으로 사용**: 복잡한 차트나 지도 기호가 필요한 경우에만 Canvas/WebGL을 사용합니다.
4. **데이터는 표준 포맷으로 마이그레이션**: 바이너리 `.dat`/`.tbl`/`.res` 파일은 JSON/IndexedDB/SQLite로 변환합니다.
5. **단계적 마이그레이션**: JKDBASE → JKWINDOW → WINDBASE → JANGO/2CAOCC 순으로 점진적으로 포팅합니다.

---

## 2. 추천 기술 스택

| 영역 | 추천 기술 | 대안 |
|------|----------|------|
| **프론트엔드 프레임워크** | React + TypeScript | Vue, Svelte, Solid |
| **상태 관리** | Zustand / Jotai | Redux, Pinia |
| **UI 컴포넌트** | Tailwind CSS + Headless UI | MUI, Ant Design |
| **지도/좌표** | Leaflet + Proj4js | OpenLayers, MapLibre |
| **차트** | Chart.js / ECharts | D3.js, Recharts |
| **클라이언트 DB** | IndexedDB + Dexie.js | SQLite WASM, PouchDB |
| **데이터 직렬화** | JSON / Protocol Buffers | MessagePack |
| **빌드 도구** | Vite | Webpack, Parcel |
| **배포** | Static Hosting + GitHub Pages | Netlify, Vercel |
| **인증** | JWT / Session Cookie | OAuth2 |

---

## 3. 단계별 포팅 계획

### Phase 0: 분석 및 데이터 마이그레이션 (4~6주)

- [ ] JKDBASE `.dat`/`.tbl` 파일 형식 완전 해독
- [ ] `RecordBase` 파생 클래스별 스키마 문서화
- [ ] `DataFileManager` 파일 형식 해독 (`.bin` 또는 8.3 파일)
- [ ] `.res` 팩 파일 형식 해독 및 이미지 추출
- [ ] 한글 코드 변환(HuboManager) 규칙 정리
- [ ] 기존 데이터 → JSON/IndexedDB 마이그레이션 스크립트 작성 (Node.js or Python)

### Phase 1: 핵심 데이터 레이어 구현 (3~4주)

- [ ] JKDBASE 개념을 TypeScript Class로 모델링
  - `EntryBase` → `Field`
  - `RecordBase` → `Record`
  - `DataManager` → `TableStore`
- [ ] IndexedDB 기반 `TableStore` 구현
  - CRUD, 검색, 정렬, 범위 검색
- [ ] `DataFileManager` 개념의 `FileStore` 구현
- [ ] `HuboManager` 대체 코드북 모듈 구현

### Phase 2: GUI 프레임워크 재구성 (4~6주)

- [ ] `JKControl` → React Component 기반 윈도우 트리
- [ ] `JKWindow`/`JKDialog` → DIV 기반 모달/윈도우
- [ ] 메시지 시스템 → React 상태 + 이벤트 핸들러
- [ ] `JKDC` → Canvas 2D Context (필요 시)
- [ ] 기본 컨트롤 구현: Button, Edit, ListBox, ComboBox, CheckBox, ScrollBar
- [ ] 리소스 매니저 → 정적 asset import + IndexedDB 캐시

### Phase 3: 업무 애플리케이션 마이그레이션 (6~10주)

- [ ] JANGO: 메인 메뉴 → SPA 라우트
- [ ] JANGO: 인사/장비/2.4G 장비 화면 구현
- [ ] JANGO: `RecordViewBase` + `EntryControl` → 폼 바인딩
- [ ] JANGO: 차트/보고서 출력
- [ ] 2CAOCC: 메뉴바 + 지도/좌표 UI
- [ ] 2CAOCC: 포대/항공/사격/진지 모듈 구현
- [ ] 2CAOCC: 좌표 변환 및 기호 표시

### Phase 4: 고급 기능 및 최적화 (3~4주)

- [ ] 오프라인 모드 (Service Worker + IndexedDB)
- [ ] 데이터 동기화 (서버 연동 시)
- [ ] 인증/권한 관리
- [ ] 프린트/PDF 출력
- [ ] 성능 최적화 (가상 스크롤, 레이지 로딩)

---

## 4. JKDBASE → 웹 데이터 모델 매핑

### 4.1 RecordBase → TypeScript Interface

```typescript
// 원본 C++
class MyRecord : public RecordBase {
    MyRecord() : RecordBase(0x1234, "MyRecord") {
        AddEntry(new NumberEntry(1, "ID", 0, 999999, 10, 0));
        AddEntry(new DateEntry(2, "Birth"));
    }
};

// 웹 TypeScript
interface MyRecord {
    id: number;
    birth: string; // ISO 8601 date
}

const myRecordSchema: RecordSchema = {
    recordId: 0x1234,
    name: "MyRecord",
    fields: [
        { entryId: 1, name: "ID", type: "number", min: 0, max: 999999, radix: 10, default: 0 },
        { entryId: 2, name: "Birth", type: "date" }
    ]
};
```

### 4.2 DataManager → TableStore

```typescript
class TableStore<T> {
    async add(record: T): Promise<number>;
    async get(index: number): Promise<T | null>;
    async set(index: number, record: T): Promise<boolean>;
    async delete(index: number): Promise<boolean>;
    async search(predicate: (r: T) => boolean, startIndex?: number): Promise<number>;
    async searchScope(min: T, max: T, fieldIds: number[]): Promise<number>;
    async sort(fieldIds: number[]): Promise<number[]>;
}
```

---

## 5. JKWINDOW → 웹 UI 매핑

| JKWINDOW | 웹 React | 비고 |
|----------|----------|------|
| `JKApplication` | `App.tsx` + 상태 관리 | 최상위 |
| `JKWindow` | `Window.tsx` (div + drag) | position: absolute |
| `JKDialog` | `Modal.tsx` | `createPortal` 사용 |
| `JKControl` | React Component | props로 메시지 핸들러 전달 |
| `JKButton` | `<button>` | |
| `JKEdit` | `<input>`, `<textarea>` | |
| `JKListBox` | `<select multiple>` 또는 가상 리스트 | |
| `JKComboBox` | `<select>` 또는 autocomplete | |
| `JKCheckBox` | `<input type="checkbox">` | |
| `JKScrollBar` | CSS overflow / 커스텀 scrollbar | |
| `JKDC` | Canvas 2D / SVG | 직접 그리기 필요 시 |

---

## 6. 2CAOCC 지도/좌표 포팅 전략

### 6.1 좌표 변환

```typescript
// 원본: real2pix.cpp
// 웹: Proj4js + Leaflet
import proj4 from 'proj4';

function realToPixel(realCoord: RealCoord, bounds: Bounds): PixelCoord {
    const x = ((realCoord.easting - bounds.minE) / (bounds.maxE - bounds.minE)) * bounds.width;
    const y = bounds.height - ((realCoord.northing - bounds.minN) / (bounds.maxN - bounds.minN)) * bounds.height;
    return { x, y };
}
```

### 6.2 지도 표시

- **Leaflet**: 오픈소스, 가벼움, 군용 좌표계 커스텀 투영 가능
- **OpenLayers**: 복잡한 지도 기능, 레이어 관리 강력
- **Canvas 직접 구현**: 단순한 schematic map 표시 시 사용

---

## 7. 리소스 마이그레이션

### 7.1 이미지/팔레트

- `.res` 팩 파일에서 개별 이미지를 추출하여 PNG/SVG로 변환
- 팔레트(`.dat`)는 CSS 색상 변수 또는 Canvas용 RGBA 배열로 변환
- 아이콘/기호는 SVG로 벡터화하여 확대/축소에 유리하게 만듦

### 7.2 한글/폰트

- 브라우저 기본 한글 폰트(Noto Sans KR, Malgun Gothic 등) 사용
- 벡터 폰트가 필요한 경우 SVG path로 변환하거나 웹폰트 활용
- `HuboManager`의 코드→텍스트 변환은 별도 JSON 코드북으로 유지

---

## 8. 인증 및 보안

| 원본 | 웹 대안 |
|------|--------|
| `PasswordDialog` + `PasswordTable` | 로그인 폼 + 서버 인증 |
| `BudaeName` 전역 상태 | 사용자 프로필/세션 상태 |
| 하드코딩된 비밀번호 | 해시 + 솔트 저장, HTTPS 전송 |

---

## 9. 위험 요소 및 주의사항

| 위험 | 설명 | 대응 |
|------|------|------|
| **데이터 손실** | 바이너리 파일 마이그레이션 중 깨짐 가능성 | 원본 백업, 검증 스크립트 작성 |
| **한글 인코딩** | EUC-KR/CP949/조합형 혼재 가능성 | 바이트 단위 분석 후 UTF-8 변환 |
| **복잡한 좌표계** | 군용 좌표계 투영 정보 부족 | 원본 `real2pix` 역산 또는 문서화 |
| **UI 재현 한계** | VESA 256색 고유 UI 느낌 | 디자인 의도는 유지하되 현대적으로 단순화 |
| **프린트 출력** | DOS 프린터 제어 코드 의존 | HTML 인쇄 또는 PDF 생성으로 대체 |

---

## 10. 권장 개발 순서 (MVP 관점)

1. **JKDBASE 해독 및 데이터 마이그레이션**이 성공해야 모든 것의 기반이 됩니다.
2. **JANGO의 2.4G 장비 관리**를 첫 번째 MVP로 삼는 것을 권장합니다.
   - 데이터 구조가 비교적 명확(`Equip24Name`/`Equip24Kind`)
   - CRUD + 검색 + 출력의 전형적인 업무 패턴을 보여줌
3. 이후 **인사 관리**, **2CAOCC 지도/좌표**로 확장합니다.

