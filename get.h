#pragma once
#include <functional>	// std::invoke
#include <tuple>		// std::tuple, std::tuple_element_t
#include <type_traits>	// std::decay_t, std::enable_if_t, std::is_base_of_v, std::is_lvalue_reference_v, std::is_void_v, std::remove_pointer_t, std::remove_reference_t, std::void_t
#include <utility>		// std::declval, std::forward

namespace sayuri {
	enum class Mode {
		Default = 0,
		None = 1,
		WRL = 2,	// Windows Runtime C++ Template Library
		ATL = 3,	// Active Template Library
		CCS = 4,	// Compiler COM Support

		Ignore = 8,
	};
	constexpr const Mode operator | (const Mode l, const Mode r) { return static_cast<Mode>(static_cast<int>(l) | static_cast<int>(r)); }

	#ifndef GET_MODE_DEFAULT
	#if defined(_WRL_CLIENT_H_)
	#define GET_MODE_DEFAULT sayuri::Mode::WRL
	#elif defined(__ATLCOMCLI_H__)
	#define GET_MODE_DEFAULT sayuri::Mode::ATL
	#elif defined(_INC_COMIP) || defined(_INC_COMUTIL)
	#define GET_MODE_DEFAULT sayuri::Mode::CCS
	#else
	#define GET_MODE_DEFAULT sayuri::Mode::None
	#endif
	#endif

	namespace details {
		constexpr const bool hasmode(const Mode mode, const Mode flag) { return (static_cast<int>(mode) & static_cast<int>(flag)) != 0; }
		constexpr const Mode libmode(const Mode mode) { return static_cast<Mode>(static_cast<int>(mode) & 0x07); }
		constexpr const Mode getmode_(const Mode mode) { return mode == Mode::Default ? GET_MODE_DEFAULT : mode; }
		constexpr const Mode getmode(const Mode mode) { return getmode_(libmode(mode)); }

		template<class Return, class... Params> struct call_info_ {
			using return_type = Return;
			static constexpr auto params_size = sizeof...(Params);
			using lastparam_type = std::tuple_element_t<sizeof...(Params) - 1, std::tuple<Params...>>;
		};
		template<class Callable> struct call_info;
		#define CALL_INFO(CALL_OPT, OPT1, OPT2) template<class Return, class... Params> struct call_info<Return (CALL_OPT*)(Params...)> : call_info_<Return, Params...> {};
		#pragma warning(suppress: 4003)	/* warning C4003: not enough actual parameters for macro '_NON_MEMBER_CALL' */
		_NON_MEMBER_CALL(CALL_INFO, );	// VS2017 takes 2 args and VS2015 takes 3 args.
		#undef CALL_INFO
		#define CALL_INFO(CALL_OPT, OPT1, OPT2) template<class Class, class Return, class... Params> struct call_info<Return (CALL_OPT Class::*)(Params...)> : call_info_<Return, Class, Params...> {};
		_MEMBER_CALL(CALL_INFO, , );
		#undef CALL_INFO

		template<class T, class TT = std::remove_reference_t<T>> inline constexpr T&& forward(TT& arg) noexcept { return static_cast<T&&>(arg); }
		template<class T, class TT = std::remove_reference_t<T>> inline constexpr T&& forward(TT&& arg) noexcept { static_assert(!std::is_lvalue_reference_v<T>, "bad forward call"); return static_cast<T&&>(arg); }
#ifdef _WRL_CLIENT_H_
		template<class T, class I> inline auto forward(Microsoft::WRL::ComPtr<I>& arg) noexcept { return arg.Get(); }
#endif

		template<class Type> struct result_info_normal {
			using result_type = Type;
			static constexpr auto get_address(result_type&& result) { return &result; }
		};
		template<class Type> struct result_info_com {
			using result_type = Type;
			static inline auto get_address(result_type&& result) { return IID_PPV_ARGS_Helper(&result); }
		};
		template<Mode mode, class Interface, class Type, class = void> struct result_info : result_info_normal<std::remove_pointer_t<Type>> { static_assert(std::is_void_v<Interface>, "Interface should be void."); };
#ifdef _WRL_CLIENT_H_
		template<class Interface> struct result_info<Mode::WRL, void, Interface**, std::enable_if_t<std::is_base_of_v<IUnknown, Interface>>> : result_info_normal<Microsoft::WRL::ComPtr<Interface>> {};
		template<class Interface> struct result_info<Mode::WRL, Interface, void**> : result_info_com<Microsoft::WRL::ComPtr<Interface>> {};
#endif
#ifdef __ATLCOMCLI_H__
		template<class Interface> struct result_info<Mode::ATL, void, Interface**, std::enable_if_t<std::is_base_of_v<IUnknown, Interface>>> : result_info_normal<ATL::CComPtr<Interface>> {};
		template<class Interface> struct result_info<Mode::ATL, Interface, void**> : result_info_com<ATL::CComPtr<Interface>> {};
		template<> struct result_info<Mode::ATL, void, BSTR*> : result_info_normal<ATL::CComBSTR> {};
		template<> struct result_info<Mode::ATL, void, VARIANT*> : result_info_normal<ATL::CComVariant> {};
#endif
#ifdef _INC_COMIP
		template<class Interface> struct result_info<Mode::CCS, void, Interface**, std::enable_if_t<std::is_base_of_v<IUnknown, Interface>>> : result_info_normal<_com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>>> {};
		template<class Interface> struct result_info<Mode::CCS, Interface, void**> : result_info_com<_com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)>>> {};
#endif
#ifdef _INC_COMUTIL
		template<> struct result_info<Mode::CCS, void, BSTR*> {
			using result_type = _bstr_t;
			static inline auto get_address(result_type&& result) { return result.GetAddress(); }
		};
		template<> struct result_info<Mode::CCS, void, VARIANT*> : result_info_normal<_variant_t> {};
#endif

		template<bool ignoreReturn> struct invoker {
			template<class Lambda> static inline void invoke(Lambda&& lambda) { check(lambda()); }
		};
		template<> struct invoker<true> {
			template<class Lambda> static inline void invoke(Lambda&& lambda) { lambda(); }
		};

		template<class Callable, Mode mode, class Interface = void, class CallInfo = call_info<std::decay_t<Callable>>, class ResultInfo = result_info<getmode(mode), Interface, typename CallInfo::lastparam_type>>
		struct info {
			using result_type = typename ResultInfo::result_type;
			template<class... Args, class = std::enable_if_t<CallInfo::params_size - sizeof...(Args) == 1>, class Invoker = invoker<hasmode(mode, Mode::Ignore) || std::is_void_v<typename CallInfo::return_type>>>
			static inline auto get(Callable&& callable, Args&&... args) {
				result_type result{};
				Invoker::invoke([&] { return std::invoke(std::forward<Callable>(callable), forward<Args>(args)..., ResultInfo::get_address(std::forward<result_type>(result))); });
				return result;
			}
#ifdef GUID_DEFINED	/* __uuidof() requires _GUID, and clang test it. */
			template<class... Args, class = std::enable_if_t<CallInfo::params_size - sizeof...(Args) == 2>>
			static inline auto get(Callable&& callable, Args&&... args) {
				return get(std::forward<Callable>(callable), std::forward<Args>(args)..., __uuidof(Interface));
			}
#endif
		};

		template<class T, class = void> struct get_interface_ { using type = T; };
		template<class T> struct get_interface_<T, std::void_t<decltype(std::declval<T>().operator->())>> { using type = decltype(std::declval<T>().operator->()); };
		template<class T> using get_interface = std::remove_pointer_t<typename get_interface_<std::decay_t<T>>::type>;
	}

	template<Mode mode = Mode::Default, class Callable, class... Args, class Info = details::info<Callable, mode>, class Result = typename Info::result_type>	// result hit for IntelliSense.
	inline Result get(Callable&& callable, Args&&... args) {
		return Info::get(std::forward<Callable>(callable), std::forward<Args>(args)...);
	}
	template<class Interface, Mode mode = Mode::Default, class Callable, class... Args, class Info = details::info<Callable, mode, Interface>, class Result = typename Info::result_type>
	inline Result get(Callable&& callable, Args&&... args) {
		return Info::get(std::forward<Callable>(callable), std::forward<Args>(args)...);
	}

#ifdef __GNUG__
	#define GET_MODE(MODE, OBJECT, METHOD, ...) sayuri::get<MODE>(&sayuri::details::get_interface<decltype(OBJECT)>::METHOD, OBJECT, ## __VA_ARGS__)
	#define GET(OBJECT, METHOD, ...) GET_MODE(sayuri::Mode::Default, OBJECT, METHOD, ## __VA_ARGS__)
	#define GETIF_MODE(INTERFACE, MODE, OBJECT, METHOD, ...) sayuri::get<INTERFACE, MODE>(&sayuri::details::get_interface<decltype(OBJECT)>::METHOD, OBJECT, ## __VA_ARGS__)
	#define GETIF(INTERFACE, OBJECT, METHOD, ...) GETIF_MODE(INTERFACE, sayuri::Mode::Default, OBJECT, METHOD, ## __VA_ARGS__)
#else
	#define GET_MODE(MODE, OBJECT, METHOD, ...) sayuri::get<MODE>(&sayuri::details::get_interface<decltype(OBJECT)>::METHOD, OBJECT, __VA_ARGS__)
	#define GET(OBJECT, METHOD, ...) GET_MODE(sayuri::Mode::Default, OBJECT, METHOD, __VA_ARGS__)
	#define GETIF_MODE(INTERFACE, MODE, OBJECT, METHOD, ...) sayuri::get<INTERFACE, MODE>(&sayuri::details::get_interface<decltype(OBJECT)>::METHOD, OBJECT, __VA_ARGS__)
	#define GETIF(INTERFACE, OBJECT, METHOD, ...) GETIF_MODE(INTERFACE, sayuri::Mode::Default, OBJECT, METHOD, __VA_ARGS__)
#endif

	static_assert(details::libmode(GET_MODE_DEFAULT) != Mode::Default, "GET_MODE_DEFAULT must not be sayuri::Mode::Default.");
}
