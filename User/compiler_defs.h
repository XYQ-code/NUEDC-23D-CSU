#ifndef COMPILER_DEFS_H
#define COMPILER_DEFS_H

#if defined ( __CC_ARM )
  #define __STATIC_FORCEINLINE static __forceinline
#elif defined ( __GNUC__ )
  #define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
#elif defined ( __ICCARM__ )
  #define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
#else
  #define __STATIC_FORCEINLINE static inline
#endif

#endif /* COMPILER_DEFS_H */
