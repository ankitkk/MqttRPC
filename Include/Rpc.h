#pragma  once

#include <tuple>
#include <type_traits>
#include <stack>
#include <sstream>

#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/complex.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/stack.hpp>

#include <iostream>
#include <fstream>
#include "Mqtt.h"

namespace rpc
{

    typedef cereal::BinaryInputArchive InputArchive;
    typedef cereal::BinaryOutputArchive OutputArchive;

    namespace detail
    {
        // setup to loop over tuple elements. 
        template<size_t I = 0, typename Func, typename ...Ts>
        typename std::enable_if<I == sizeof...(Ts)>::type
            for_each_in_tuple(std::tuple<Ts...> &, Func) {}

        template<size_t I = 0, typename Func, typename ...Ts>
        typename std::enable_if < I < sizeof...(Ts)>::type
            for_each_in_tuple(std::tuple<Ts...> & tpl, Func func)
        {
            func(std::get<I>(tpl));
            for_each_in_tuple<I + 1>(tpl, func);
        }
        
        typedef std::stack<std::vector<uint8_t>> ArgumentSourceType;

        // Wire->DataType. 
        template<class T>
        inline auto get(ArgumentSourceType& args)
        {
            // T must be default constructible
            std::vector<uint8_t>  Source = args.top();
            args.pop();

            typename std::remove_const<typename std::remove_reference<T>::type>::type val;

            std::stringstream ssout(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
            {
                ssout.write((char*)Source.data(), Source.size());
                InputArchive input(ssout); // stream to cout
                input(cereal::make_nvp("mqttrpc", val));
            }

            return val; 
        }

        // Really Helpful.
        // http://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda

        template <typename T>
        struct function_traits
            : public function_traits<decltype(&T::operator())>
        {};
        // For generic types, directly use the result of the signature of its 'operator()'

        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
            // we specialize for pointers to member function
        {
            enum { arity = sizeof...(Args) };
            // arity is the number of arguments.

            typedef ReturnType result_type;

            template <size_t i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
                // the i-th argument is equivalent to the i-th tuple element of a tuple
                // composed of those arguments.
            };
        };

        // for proper evaluation of the stream extraction to the arguments
        template<class R>
        struct invoker {
            R result;
            template<class F, class... Args>
            invoker(F&& f, Args&&... args)
                : result(f(std::forward<Args>(args)...)) {}
        };

        template<>
        struct invoker<void> {
            template<class F, class... Args>
            invoker(F&& f, Args&&... args)
            {
                f(std::forward<Args>(args)...);
            }
        };

        template<class F, class Sig>
        struct stream_function_;

        template<class F, class R, class... Args>
        struct stream_function_<F, R(Args...)> {
            stream_function_(F f)
                : _f(f) {}

            void operator()(ArgumentSourceType& args, std::string* out_opt) const {
                call(args, out_opt, std::is_void<R>());
            }

        private:
          
            // void return
            void call(ArgumentSourceType& args, std::string*, std::true_type) const {
                _f(get<Args>(args)...);
            }

            // non-void return
            void call(ArgumentSourceType& args, std::string* out_opt, std::false_type) const {
                if (!out_opt) // no return wanted, redirect
                    return call(args, nullptr, std::true_type());

                std::stringstream conv;
                if (!(conv << invoker<R>{_f, get<Args>(args)...}.result))
                    throw std::runtime_error("bad return in stream_function");
                *out_opt = conv.str();
            }

            F _f;
        };

        template<class Sig, class F>
        stream_function_<F, Sig> stream_function(F f) { return { f }; }
    }


    template<typename T>
    inline void serialize(T& object, std::stringstream& ss)
    {
        OutputArchive ar(ss);
        ar(object);
    }
    // handle const char*.
    // implicityl convert to std::string.
    inline void serialize(const char*& object, std::stringstream& ss)
    {
        OutputArchive ar(ss);
        std::string str(object);
        ar(str);
    }

    class PeerConnection
    {

    public:
        void Init(const std::string my_topic, const std::string peer_topic);  

        template <typename... Args>
        void Call(const std::string& func_name, Args... args)
        {
            detail::ArgumentSourceType stack;

            auto serializer = [&](auto object) {
                //  serialize Val. 
                std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
                // 
                serialize(object, ss);
                // copy the stringstream to a byte vector. 
                std::vector<uint8_t> s;
                std::copy(std::istreambuf_iterator<char>(ss), std::istreambuf_iterator<char>(), std::back_inserter(s));
                stack.push(s);
            };

            // make a tuple pack of the arguments. 
            auto tuple_pack = std::make_tuple(std::forward<Args>(args)...);
            // loop through all arguments and serialize. 
            detail::for_each_in_tuple(tuple_pack, serializer);

            // push function name.
            stack.push(std::vector<uint8_t>(func_name.begin(), func_name.end()));
            //  serialize stack. 
            std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
            {
                cereal::BinaryOutputArchive output(ss);
                output(stack);
            }
            // copy the stringstream to the payload.
            shared::PayLoadPtr payload(new shared::PayLoadType());
            std::copy(std::istreambuf_iterator<char>(ss), std::istreambuf_iterator<char>(), std::back_inserter(*payload));
            // put the payload on the wire.
            mqtt::MQTT::Instance().PublishAsync( peer_topic + "/" + my_topic, std::move(payload));
        }


        typedef std::function<void(detail::ArgumentSourceType&, std::string*)> func_type;
        typedef std::unordered_map<std::string, func_type> dict_type;


        template <typename Functor>
        typename std::enable_if<detail::function_traits<Functor>::arity == 0>::type BindImpl(const std::string& FuncName, Functor F) 
        {
            typedef typename detail::function_traits<Functor> traits;
            function_registry[FuncName] = detail::stream_function<typename traits::result_type ,void>(F);
        }

        template <typename Functor>
        typename std::enable_if<detail::function_traits<Functor>::arity == 1>::type BindImpl(const std::string& FuncName, Functor F) 
        {
            typedef typename detail::function_traits<Functor> traits;
            function_registry[FuncName] = detail::stream_function<typename traits::result_type(typename traits::template arg<0>::type)>(F);
        }

        template <typename Functor>
        typename std::enable_if<detail::function_traits<Functor>::arity == 2>::type BindImpl(const std::string& FuncName, Functor F) 
        {
            typedef typename detail::function_traits<Functor> traits;
            function_registry[FuncName] = detail::stream_function<typename  traits::result_type(typename  traits::template arg<0>::type, typename  traits::template arg<1>::type)>(F);
        }

        template <typename Functor>
        typename std::enable_if<detail::function_traits<Functor>::arity == 3>::type BindImpl(const std::string& FuncName, Functor F)
        {
            typedef typename detail::function_traits<Functor> traits;
            function_registry[FuncName] = detail::stream_function<typename  traits::result_type(typename  traits::template arg<0>::type, typename  traits::template arg<1>::type, typename  traits::template arg<2>::type)>(F);
        }

        template <typename Functor>
        typename std::enable_if<detail::function_traits<Functor>::arity == 4>::type BindImpl(const std::string& FuncName, Functor F) 
        {
            typedef typename detail::function_traits<Functor> traits;
            function_registry[FuncName] = detail::stream_function<typename  traits::result_type(traits::template arg<0>::type, traits::template arg<1>::type, traits::template arg<2>::type, traits::template arg<3>::type)>(F);
        }


        template <typename Functor>
        void Bind(const std::string& FuncName, Functor F) 
        {
            typedef detail::function_traits<Functor> traits;
            BindImpl(FuncName, F);
        }

        static std::string source_topic_in_progress;

    private:
        
        std::string     my_topic;
        std::string     peer_topic;
        dict_type       function_registry;

    };
}

