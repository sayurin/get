# sayuri::get()

`sayuri::get()`はWindows APIやCOMインターフェースの呼び出しを補助するユーティリティ関数です。

## Introduction

Windows APIには

    HRESULT CoCreateGuid(
      _Out_ GUID *pguid
    );


のように関数の戻り値としては`HRESULT`のようなエラー値、実際の戻り値は関数の最終引数としたAPIが存在します。このようなAPIを呼び出すには

    GUID guid;
    auto hr = CoCreateGuid(&guid);
    if (FAILED(hr)) throw hr;

といったコーディングパターンとなりますが、API呼び出し一つ一つがこのように繰り返されると非常に煩雑で、コードの見通しが悪くなります。`sayuri::get()`はC++コンパイラーの型推論を駆使することで

    auto guid = sayuri::get(CoCreateGuid);

とAPI呼び出しを代替するコード記述を提供します。

## Usage

    HRESULT CoCreateGuid(
      _Out_ GUID *pguid
    );

に対して

    auto guid = sayuri::get(CoCreateGuid);

と記述すると

    GUID guid;
    auto hr = CoCreateGuid(&guid);
    check(hr);

に展開されます。呼び出した関数の戻り値型に応じた`void check(<returned type>)`関数を用意してください。（上記の例では`void check(HRESULT)`を呼び出します。）

当然ながら呼び出した関数の戻り値型が`void`の場合は`check()`を呼び出しません。

### 戻り値の破棄

`sayuri::get()`は型引数でモードを指定することができます。`sayuri::Mode::Ignore`を指定すると戻り値を破棄し、`check()`を呼び出さなくなります。

    auto guid = sayuri::get<sayuri::Mode::Ignore>(CoCreateGuid);

と記述すると

    GUID guid;
    CoCreateGuid(&guid);

に展開します。

### 任意引数

`sayuri::get()`は任意引数に対応しています。`std::invoke()`の要領で、関数名に続けて渡したい引数を並べることができます。

    HRESULT CLSIDFromProgID(
      _In_  LPCOLESTR lpszProgID,
      _Out_ LPCLSID   lpclsid
    );

に対して

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");

と記述すると

    CLSID clsid;
    auto hr = CLSIDFromProgID(L"Shell.Application", &clsid);
    check(hr);

に展開します。

### COMインスタンス生成

COMインスタンスを生成するためには`CoCreateInstance()`を使用しますが、プロトタイプ宣言は

    HRESULT CoCreateInstance(
      _In_  REFCLSID  rclsid,
      _In_  LPUNKNOWN pUnkOuter,
      _In_  DWORD     dwClsContext,
      _In_  REFIID    riid,
      _Out_ LPVOID    *ppv
    );

です。通常の呼び出し方法であれば

    HRESULT hr;
    CLSID clsid;
    hr = CLSIDFromProgID(L"Shell.Application", &clsid);
    if (FAILED(hr)) throw hr;
    IShellDispatch2* shell;
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&shell));
    if (FAILED(hr)) throw hr;

このようになります。ここで最終引数`LPVOID* ppv`からは`IShellDispatch2*`を型推論することができません。そのため`sayuri::get()`では明示的に型引数を受け取るオーバーロードを用意しています。

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL, __uuidof(IShellDispatch2));

と書けます。更に`__uuidof()`を自動的に補完することもでき

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);

と呼び出すこともできます。

これは`CoCreateInstance()`に限りません。`IID_PPV_ARGS()`を使用可能な関数、つまり`REFIID`を受け取り`LPVOID*`を返す関数に適用できます。例えば`CreateDXGIFactory()`もその一つです。

    HRESULT CreateDXGIFactory(
            REFIID riid,
      _Out_ void   **ppFactory
    );

に対して

    auto factory = sayuri::get<IDXGIFactory>(CreateDXGIFactory);

と書けます。

### COMライブラリ

Visual C++では各種ライブラリによるCOMスマートポインターが用意されています。`sayuri::get()`は

 - [Windows Runtime C++ Template Library](https://msdn.microsoft.com/ja-jp/library/hh438466.aspx)
 - [Active Template Library](https://msdn.microsoft.com/ja-jp/library/t9adwcde.aspx)
 - [Compiler COM Support](https://msdn.microsoft.com/ja-jp/library/h31ekh7e.aspx)

に対応しています。

戻り値がCOMインターフェースとなる場合に`sayuri::Mode::WRL`を指定するとWindows Runtime C++ Template Libraryクラスを使用します。

    auto shell = sayuri::get<IShellDispatch2, sayuri::Mode::WRL>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);

の場合、`shell`の型は`Microsoft::WRL::ComPtr<IShellDispatch2>`になります。

`sayuri::Mode::ATL`を指定するとActive Template Libraryクラスを使用します。

    auto shell = sayuri::get<IShellDispatch2, sayuri::Mode::ATL>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);

の場合、`shell`の型は`ATL::CComPtr<IShellDispatch2>`になります。

`sayuri::Mode::CCS`を指定するとCompiler COM Supportクラスを使用します。

    auto shell = sayuri::get<IShellDispatch2, sayuri::Mode::CCS>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);

の場合、`shell`の型は`_com_ptr_t`を使用した`IShellDispatch2Ptr`になります。

`sayuri::Mode::None`を指定するとCOMライブラリを使用しません。

    auto shell = sayuri::get<IShellDispatch2, sayuri::Mode::CCS>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);

の場合、`shell`の型は`IShellDispatch2*`になります。

`GET_MODE_DEFAULT`マクロ定数を使用してデフォルトで使用するCOMライブラリを指定できます。

に展開されます。

### BSTRとVARIANT

`BSTR`と`VARIANT`についてもCOMサポートライブラリが用意されています。

- `sayuri::Mode::ATL`を指定した場合
  - `BSTR`には`ATL::CComBSTR`を使用します。
  - `VARIANT`には`ATL::CComVARIANT`を使用します。
- `sayuri::Mode::CCS`を指定した場合
  - `BSTR`には`_bstr_t`を使用します。
  - `VARIANT`には`_variant_t`を使用します。

### COMインターフェース呼び出し

例えば`IShellDispatch2::GetSystemInformation()`は次のような定義となっています。

    HRESULT GetSystemInformation(
      [in]          BSTR     name,
      [retval][out] VARIANT *pv
    );

これを呼び出すには

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);
    _variant_t bytes;
    auto hr = shell->GetSystemInformation(_bstr_t{ L"PhysicalMemoryInstalled" }, &bytes);
    check(hr);

となると思いますが、`sayuri::get()`を使う場合は

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);
    auto bytes = sayuri::get(&IShellDispatch2::GetSystemInformation, shell, _bstr_t{ L"PhysicalMemoryInstalled" });

とメンバー関数ポインター`&IShellDispatch2::GetSystemInformation`を記述する必要があり煩雑です。これを緩和するため`GET()`マクロを用意しています。

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);
    auto bytes = GET(shell, GetSystemInformation, _bstr_t{ L"PhysicalMemoryInstalled" });

と`GET()`マクロには第１引数にオブジェクト、第２引数にメンバー関数名、それ以降には引数を記述することができます。この例では`bytes`の型は`_variant_t`となります。またモード指定可能な`GET_MODE()`マクロも用意しています。

    auto clsid = sayuri::get(CLSIDFromProgID, L"Shell.Application");
    auto shell = sayuri::get<IShellDispatch2>(CoCreateInstance, clsid, nullptr, CLSCTX_ALL);
    auto bytes = GET_MODE(sayuri::Mode::ATL, shell, GetSystemInformation, _bstr_t{ L"PhysicalMemoryInstalled" });

この例では`bytes`の型は`ATL::CComVARIANT`となります。

## Reference

### `sayuri::Mode`

    namespace sayuri {
      enum class Mode {
        Default,    // GET_MODE_DEFAULTマクロ定数に従う
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

 - `sayuri::get()`では`NULL`を使用できません。`NULL`は仕様により`0`と定義されています。`0`はポインター型へ代入可能ではありますが、型推論では`int`型と判断されてしまいます。そして`int`型はポインター型へ代入できないため、オーバーロード解決に失敗します。この問題を回避するために`nullptr`を使用してください。
 - 関数オーバーロードが存在する場合、関数ポインターを作成するのは煩雑です。更にメンバー関数名にオーバーロードが存在する場合、`GET()`マクロが使用できないことが多いです。

## Environment

次のコンパイラー環境で動作します。

 - Visual Studio 2017 (v141)
 - Visual Studio 2017 - Clang with Microsoft CodeGen (v141_clang_c2)
 - Visual Studio 2015 (v140)
 - Visual Studio 2015 - Clang with Microsoft CodeGen (v140_clang_c2)

ただし、Visual Studio 2015ではIntelliSenseが動作せずクラッシュします。

## Auther

sayurin <https://github.com/sayurin>
