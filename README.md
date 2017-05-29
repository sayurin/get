# sayuri::get()

`sayuri::get()`はWindows APIやCOMインターフェースの呼び出しを補助するユーティリティ関数です。

## Introduction

Windows APIには

    HRESULT Direct3DCreate9Ex(
      _In_  UINT         SDKVersion,
      _Out_ IDirect3D9Ex **ppD3D
    );

のように関数の戻り値としては`HRESULT`のようなエラー値、実際の戻り値は関数の最終引数としたAPIが存在します。またCOMインターフェースも同様です。このようなAPIを呼び出すには

    IDirect3D9Ex* pD3D;
    auto result = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
    if (FAILED(result)) throw result;

といったコーディングパターンとなりますが、API呼び出し一つ一つがこのように繰り返されると非常に煩雑で、コードの見通しが悪くなります。`sayuri::get()`はC++コンパイラーの型推論を駆使することで

    auto pD3D = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);

とAPI呼び出しを代替し、簡単なコード記述を提供します。

## Usage

    HRESULT Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D);

に対して

    auto pD3D = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);

と記述すると

    IDirect3D9Ex* pD3D;
    auto _result = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
    check(_result);

に展開されます。呼び出した関数の戻り値型に応じた`void check(RESULT)`関数を用意してください。

### COMライブラリ

Visual C++では各種ライブラリによるCOMスマートポインターが用意されています。`sayuri::get()`は

 - [Windows Runtime C++ Template Library](https://msdn.microsoft.com/ja-jp/library/hh438466.aspx)
 - [Active Template Library](https://msdn.microsoft.com/ja-jp/library/t9adwcde.aspx)
 - [Compiler COM Support](https://msdn.microsoft.com/ja-jp/library/h31ekh7e.aspx)

に対応してます。`sayuri::Mode::WRL`を指定すると

    auto d3d = sayuri::get<sayuri::Mode::WRL>(Direct3DCreate9Ex, D3D_SDK_VERSION);
    // ↓
    Microsoft::WRL::ComPtr<IDirect3D9Ex> d3d;
    auto _result = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);
    check(_result);

に展開され、`sayuri::Mode::ATL`を指定すると

    auto d3d = sayuri::get<sayuri::Mode::ATL>(Direct3DCreate9Ex, D3D_SDK_VERSION);
    // ↓
    ATL::CComPtr<IDirect3D9Ex> d3d;
    auto _result = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);
    check(_result);

に展開され、`sayuri::Mode::CCS`を指定すると

    auto d3d = sayuri::get<sayuri::Mode::CCS>(Direct3DCreate9Ex, D3D_SDK_VERSION);
    // ↓
    IDirect3D9ExPtr d3d;
    auto _result = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);
    check(_result);

に展開されます。`sayuri::Mode::Ignore`を指定すると戻り値を無視し`check()`の呼び出しを抑止できます。

    auto d3d = sayuri::get<sayuri::Mode::WRL|sayuri::Mode::Ignore>(Direct3DCreate9Ex, D3D_SDK_VERSION);
    // ↓
    Microsoft::WRL::ComPtr<IDirect3D9Ex> d3d;
    Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d);

### COMインターフェース呼び出し

例えば`IDirect3D9Ex::CreateDeviceEx()`は次のような定義となっています。

    HRESULT CreateDeviceEx(
      [in]          UINT                  Adapter,
      [in]          D3DDEVTYPE            DeviceType,
      [in]          HWND                  hFocusWindow,
      [in]          DWORD                 BehaviorFlags,
      [in, out]     D3DPRESENT_PARAMETERS *pPresentationParameters,
      [in, out]     D3DDISPLAYMODEEX      *pFullscreenDisplayMode,
      [out, retval] IDirect3DDevice9Ex    **ppReturnedDeviceInterface
    );

これを呼び出すには

    auto d3d = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp{};
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> device;
    auto _result = d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, 0, &pp, nullptr, &device);
    check(_result);

となると思いますが、`sayuri::get()`を使う場合は

    auto d3d = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp{};
    auto device = sayuri::get(&IDirect3D9Ex::CreateDeviceEx, &d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, 0, &pp, nullptr);

とメンバー関数ポインター`&IDirect3D9Ex::CreateDeviceEx`を記述する必要があり煩雑です。これを緩和するため`GET()`マクロを用意しています。

    auto d3d = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp{};
    auto device = GET(d3d, CreateDeviceEx, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, 0, &pp, nullptr);

と`GET()`マクロには第１引数にオブジェクト、第２引数にメンバー関数名、それ以降には引数を記述することができます。またモード指定可能な`GET_MODE()`マクロも用意しています。

    auto d3d = sayuri::get(Direct3DCreate9Ex, D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp{};
    auto device = GET_MODE(sayuri::Mode::ATL, d3d, CreateDeviceEx, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, 0, &pp, nullptr);

### COMインスタンス生成

COMインスタンスを生成するためには`CoCreateInstance()`を使用しますが、プロトタイプ宣言は

    HRESULT CoCreateInstance(
      _In_  REFCLSID  rclsid,
      _In_  LPUNKNOWN pUnkOuter,
      _In_  DWORD     dwClsContext,
      _In_  REFIID    riid,
      _Out_ LPVOID    *ppv
    );

です。通常の呼び出しは`IID_PPV_ARGS()`マクロを使用して`REFIID`と`LPVOID*`との両方を指定でき、

    Microsoft::WRL::ComPtr<IFileDialog> fileDialog;
    auto _result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fileDialog);
    check(_result);

となります。`sayuri::get()`を使う場合、

    auto fileDialog = sayuri::get<IFileDialog>(CoCreateInstance, CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER);

のように型引数で明示的にインターフェースを指定することができます。もちろん、`CreateDXGIFactory()`など類似する関数にも使えます。

    auto factory = sayuri::get<IDXGIFactory>(CreateDXGIFactory);

`IUnknown::QueryInterface()`や`IServiceProvider::QueryService()`などのインターフェース呼び出しでも同様にインターフェースを指定する必要があるため、`GETIF()`マクロと`GETIF_MODE()`マクロを用意しています。

### BSTR

`BSTR`についてもCOMサポートライブラリが用意されています。`sayuri::Mode::ATL`を指定すると`ATL::CComBSTR`が、`sayuri::Mode::CCS`を指定すると`_bstr_t`が返されます。

## Reference

### `sayuri::Mode`

    namespace sayuri {
      enum class Mode {
        Default,    // GET_MODE_DEFAULT定数に従う
        None,       // COMサポートライブラリを使用しない
        WRL,        // Windows Runtime C++ Template Libraryを使用する
        ATL,        // Active Template Libraryを使用する
        CCS,        // Compiler COM Supportを使用する
        Ignore,     // 戻り値を無視し、check()関数を呼び出さない
      };
    }

### `GET_MODE_DEFAULT`

デフォルトのモードを指定します。未指定の場合、`WRL`、`ATL`、`CCS`、`None`の順で自動選択されます。

### `sayuri::get`

    namespace sayuri {
        template<Mode mode = Mode::Default, class Callable, class... Args>
        inline auto get(Callable&& callable, Args&&... args);

        template<class Interface, Mode mode = Mode::Default, class Callable, class... Args>
        inline auto get(Callable&& callable, Args&&... args);
    }

### `GET`、`GET_MODE`、`GETIF`、`GETIF_MODE`

    #define GET(OBJECT, METHODNAME, ...)
    #define GET_MODE(MODE, OBJECT, METHODNAME, ...)
    #define GETIF(INTERFACE, OBJECT, METHODNAME, ...)
    #define GETIF_MODE(INTERFACE, MODE, OBJECT, METHODNAME, ...)

## Restriction

`sayuri::get()`では`NULL`を使用できません。`NULL`は仕様により`0`と定義されています。`0`はポインター型へ代入可能ではありますが、型推論では`int`型と判断されてしまいます。そして`int`型はポインター型へ代入できないため、オーバーロード解決に失敗します。この問題を回避するために`nullptr`を使用してください。

## Environment

Visual Studio 2017

## Auther

sayurin <https://github.com/sayurin>
