// NuvolaEventSystem (MIT, github.com/DisabledMallis/NuvolaEventSystem, commit a7b2880), with a fault guard around
// each listener call added for Monchi. The listener storage is Monchi's as well: upstream keeps a map of vectors behind
// a shared lock that a dispatch takes twice and that a listener which listens or deafens during its own event takes
// a third time, exclusively; both freeze the thread. Here a dispatch runs over an immutable snapshot without a lock.
#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <iostream>

#if defined(__clang__) or defined(__GNUC__)
#define NES_FUNCSIG __PRETTY_FUNCTION__
#elif _MSC_VER
#define NES_FUNCSIG __FUNCSIG__
#endif

/*
* nes - Nuvola Event System
 */
namespace nes {
	// Monchi: Flarial's modules run on game threads with offsets that may be wrong for the running version. A
	// listener that faults or throws is skipped from then on and reported, instead of taking the game down.
	namespace listener_guard {
		inline std::mutex lock;
		inline std::vector<void*> broken;
		inline std::atomic<int> brokenCount{0};
		inline void (*onFault)(void* instance) = nullptr;

		// asked before every listener call on every thread; as long as nothing has faulted there is no lock to take
		inline bool faulted(void* instance) {
			if (!instance || brokenCount.load(std::memory_order_relaxed) == 0) return false;
			std::scoped_lock g(lock);
			for (void* b : broken)
				if (b == instance) return true;
			return false;
		}

		inline void mark(void* instance) {
			{
				std::scoped_lock g(lock);
				broken.push_back(instance);
				brokenCount.fetch_add(1, std::memory_order_relaxed);
			}
			if (onFault) onFault(instance);
		}

		inline bool seh(void (*fn)(void*), void* ctx) {
			__try {
				fn(ctx);
				return true;
			} __except (1) {
				return false;
			}
		}

		inline void run(void* instance, void (*fn)(void*), void* ctx) {
			bool ok = false;
			try {
				ok = seh(fn, ctx);
			} catch (...) {
				ok = false;
			}
			if (!ok && instance) mark(instance);
		}
	}

	namespace detail {
		constexpr std::uint32_t fnv1a_hash(std::string_view str) {
			std::uint32_t hash = 2166136261u;
			for (std::size_t i = 0; i < str.length(); ++i) {
				hash ^= static_cast<std::uint32_t>(str[i]);
				hash *= 16777619u;
			}
			return hash;
		}

		template<typename type_t>
		struct type_info {
			static constexpr std::string_view name() {
				constexpr std::string_view sig = NES_FUNCSIG;
				constexpr auto cut = sig.substr(sig.find("[type_t = ") + 10);
				return cut.substr(0, cut.find(']'));
			}
		};

		template<typename type_t>
		struct type_hash {
			[[nodiscard]] static constexpr std::size_t value() {
				constexpr auto name = type_info<type_t>::name();
				return fnv1a_hash(name);
			}
		};

		//Utils to extract type information
		template<typename class_t = std::false_type>
		struct extract_type {
			typedef class_t type;
		};

		template<typename return_t, typename class_t, typename... args_t>
		struct extract_type<return_t (class_t::*)(args_t...)> {
			typedef class_t type;
			typedef return_t ret;
		};

		enum class standard_event_priority {
			FIRST,
			NORMAL,
			LAST
		};
	}// namespace detail

	template<typename priority_t> struct event_priority_traits { using priority_type = priority_t; };

#ifndef NES_PRIORITY_TYPE
	using event_priority = detail::standard_event_priority;
	template<> struct event_priority_traits<event_priority> { using priority_type = event_priority; static constexpr priority_type default_value = priority_type::NORMAL; };
#else
	using event_priority = NES_PRIORITY_TYPE;
#ifndef NES_PRIORITY_TRAITS
	template<> struct event_priority_traits<event_priority> { using priority_type = event_priority; static constexpr priority_type default_value = priority_type::NORMAL; };
#else
	// Define your own priority traits
	NES_PRIORITY_TRAITS;
#endif
#endif

	//Owns an event pointer, used to pass around an event
	template<typename event_t>
	struct event_holder {
		template<typename... args_t>
		explicit event_holder(args_t&&... args) : mEvent(std::forward<args_t>(args)...){};

		event_t* get() {
			return &mEvent;
		}
		event_t& ref() {
			return mEvent;
		}
		event_t* operator->() {
			return get();
		}

	private:
		event_t mEvent;
	};

	//Creates an event holder instance
	template<typename event_t, typename... args_t>
	event_holder<event_t> make_holder(args_t&&... args) {
		return event_holder<event_t>(std::forward<args_t>(args)...);
	}

	//A type for holding the wrapper function that invokes the callback
	template<typename event_t>
	using event_wrapper = std::function<void(event_t&)>;

	template<typename event_t, typename wrapper_t = event_wrapper<event_t>>
	struct event_listener {
		using holder_t = event_holder<event_t>;

		event_listener() = delete;
		event_listener(void* instance, wrapper_t&& wrapper, const std::size_t methodHash) : mInstance(instance), mMethod(std::move(wrapper)), mMethodHash(methodHash){};

		void invoke(holder_t& holder) const {
			if (listener_guard::faulted(mInstance)) return;
			struct call_t { const wrapper_t* method; event_t* event; } call{&mMethod, &holder.ref()};
			listener_guard::run(mInstance, [](void* p) {
				auto* c = static_cast<call_t*>(p);
				(*c->method)(*c->event);
			}, &call);
		}

		void* mInstance = nullptr;
		wrapper_t mMethod{};
		std::size_t mMethodHash = 0;
	};

	template<typename event_t>
	struct dispatcher {
		using type_t = event_t;
		using id_t = detail::type_hash<event_t>;
		using holder_t = event_holder<event_t>;
		using listener_t = event_listener<event_t>;

		template<auto handler, auto priority = event_priority_traits<event_priority>::default_value, typename class_t = typename detail::extract_type<decltype(handler)>::class_t, typename wrapper_t = event_wrapper<event_t>>
		void listen(class_t* instance) {
			wrapper_t wrapper = [instance](event_t& e) {
				(instance->*handler)(e);
			};
			add(priority, listener_t(instance, std::move(wrapper), detail::type_hash<decltype(handler)>::value()));
		}
		template<auto priority = event_priority_traits<event_priority>::default_value, typename wrapper_t = event_wrapper<event_t>>
		void listen(auto handler) {
			wrapper_t wrapper = [handler](event_t& e) {
				handler(e);
			};
			add(priority, listener_t(nullptr, std::move(wrapper), detail::type_hash<decltype(handler)>::value()));
		}
		template<typename handler_t, typename class_t = typename detail::extract_type<handler_t>::class_t>
		void deafen(class_t* instance, handler_t&& handler) {
			remove([&](const listener_t& listener) { return listener.mInstance == instance && listener.mMethodHash == detail::type_hash<handler_t>::value(); });
		}
		template<typename handler_t>
		void deafen(handler_t handler) {
			remove([&](const listener_t& listener) { return listener.mMethodHash == detail::type_hash<handler_t>::value(); });
		}
		// A listener taken out while this runs on another thread can still be called once more from the snapshot that
		// thread holds; Flarial's listeners check their own enabled state, and modules are never destroyed.
		bool empty() const { return mList.load()->empty(); }

		void trigger(holder_t& holder) {
			auto list = mList.load();
			for (const auto& slot : *list) slot.listener.invoke(holder);
		}

	private:
		struct slot_t {
			event_priority priority;
			listener_t listener;
		};
		using list_t = std::vector<slot_t>;

		// kept in the order the map of priorities used to give: by priority, then by when the listener was added
		void add(event_priority priority, listener_t&& listener) {
			std::scoped_lock g(mWrite);
			auto next = std::make_shared<list_t>(*mList.load());
			auto at = std::upper_bound(next->begin(), next->end(), priority, [](event_priority p, const slot_t& s) { return p < s.priority; });
			next->insert(at, slot_t{priority, std::move(listener)});
			mList.store(std::shared_ptr<const list_t>(std::move(next)));
		}
		template<typename pred_t>
		void remove(pred_t&& gone) {
			std::scoped_lock g(mWrite);
			auto next = std::make_shared<list_t>(*mList.load());
			std::erase_if(*next, [&](const slot_t& s) { return gone(s.listener); });
			mList.store(std::shared_ptr<const list_t>(std::move(next)));
		}

		std::mutex mWrite;
		std::atomic<std::shared_ptr<const list_t>> mList{std::make_shared<const list_t>()};
	};

	//The main event dispatcher, use this to listen for and dispatch events
	struct event_dispatcher {
		template<typename event_t>
		[[nodiscard]] auto& get() const {
			static dispatcher<event_t> instance;
			return instance;
		}

		template<typename event_t>
		void trigger(event_holder<event_t>& e) const {
			auto& disp = get<event_t>();
			disp.trigger(e);
		}

		template<typename event_t, auto handler, auto priority = event_priority_traits<event_priority>::default_value, typename class_t = typename detail::extract_type<decltype(handler)>::class_t>
		void listen(class_t* instance) const {
			auto& disp = get<event_t>();
			disp.template listen<handler, priority>(instance);
		}
		template<typename event_t, auto priority = event_priority_traits<event_priority>::default_value>
		void listen(auto handler) const {
			auto& disp = get<event_t>();
			disp.template listen<priority>(handler);
		}

		template<typename event_t, auto handler, typename class_t = typename detail::extract_type<decltype(handler)>::class_t>
		void deafen(class_t* instance) const {
			auto& disp = get<event_t>();
			disp.deafen(instance, static_cast<decltype(handler)>(handler));
		}
		template<typename event_t>
		void deafen(auto handler) const {
			auto& disp = get<event_t>();
			disp.deafen(handler);
		}
	};

	template<typename event_t, auto handler, auto priority = event_priority_traits<event_priority>::default_value, typename dispatcher_t = event_dispatcher>
	struct scoped_listener {
		constexpr explicit scoped_listener(dispatcher_t& disp) : mDispatcher{ disp } {
			mDispatcher.template listen<event_t, &scoped_listener::listener, priority>(this);
		}
		constexpr ~scoped_listener() {
			mDispatcher.template deafen<event_t, &scoped_listener::listener>(this);
		}
		constexpr void listener(event_t& e) const {
			handler(e);
		}
	private:
		dispatcher_t& mDispatcher;
	};
}// namespace nes
