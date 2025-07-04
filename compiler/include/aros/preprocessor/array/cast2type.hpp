# ifndef AROS_PREPROCESSOR_ARRAY_CAST2TYPE_HPP
# define AROS_PREPROCESSOR_ARRAY_CAST2TYPE_HPP
# 
# include <boost/preprocessor/repetition/repeat.hpp>
# include <boost/preprocessor/array.hpp>
# include <boost/preprocessor/comparison/equal.hpp>
# include <boost/preprocessor/arithmetic/inc.hpp>
# include <boost/preprocessor/logical/compl.hpp>
# include <boost/preprocessor/punctuation/comma_if.hpp>
#
# define AROS_PP_ARRAY_CAST2TYPE_M(z, i, data)                                      \
      (BOOST_PP_TUPLE_ELEM(2, 0, data))(BOOST_PP_ARRAY_ELEM(i,BOOST_PP_TUPLE_ELEM(2, 1, data)))BOOST_PP_COMMA_IF(                         \
          BOOST_PP_COMPL(BOOST_PP_EQUAL(BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(2, 1, data)),BOOST_PP_INC(i))) \
      )                                                                              \
      /**/
#
# define AROS_PP_ARRAY_CAST2TYPE(type, array)                                             \
      BOOST_PP_REPEAT(BOOST_PP_ARRAY_SIZE(array), AROS_PP_ARRAY_CAST2TYPE_M, (type, array)) \
      /**/
#
#endif

    
