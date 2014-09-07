#ifndef __KPATCH_MACROS_H_
#define __KPATCH_MACROS_H_

#include <linux/compiler.h>
#include <linux/jiffies.h>
#include "kpatch.h"

typedef void (*kpatch_loadcall_t)(void);
typedef void (*kpatch_unloadcall_t)(void);

struct kpatch_load {
	kpatch_loadcall_t fn;
	char *objname; /* filled in by create-diff-object */
};

struct kpatch_unload {
	kpatch_unloadcall_t fn;
	char *objname; /* filled in by create-diff-object */
};

/*
 * KPATCH_IGNORE_SECTION macro
 *
 * This macro is for ignoring sections that may change as a side effect of
 * another change or might be a non-bundlable section; that is one that does
 * not honor -ffunction-section and create a one-to-one relation from function
 * symbol to section.
 */
#define KPATCH_IGNORE_SECTION(_sec) \
	char *__UNIQUE_ID(kpatch_ignore_section_) __section(.kpatch.ignore.sections) = _sec;

/*
 * KPATCH_IGNORE_FUNCTION macro
 *
 * This macro is for ignoring functions that may change as a side effect of a
 * change in another function.  The WARN class of macros, for example, embed
 * the line number in an instruction, which will cause the function to be
 * detected as changed when, in fact, there has been no functional change.
 */
#define KPATCH_IGNORE_FUNCTION(_fn) \
	void *__kpatch_ignore_func_##_fn __section(.kpatch.ignore.functions) = _fn;

/*
 * KPATCH_LOAD_HOOK macro
 *
 * The first line only ensures that the hook being registered has the required
 * function signature.  If not, there is compile error on this line.
 *
 * The section line declares a struct kpatch_load to be allocated in a new
 * .kpatch.hook.load section.  This kpatch_load_data symbol is later stripped
 * by create-diff-object so that it can be declared in multiple objects that
 * are later linked together, avoiding global symbol collision.  Since multiple
 * hooks can be registered, the .kpatch.hook.load section is a table of struct
 * kpatch_load elements that will be executed in series by the kpatch core
 * module at load time, assuming the kernel object (module) is currently
 * loaded; otherwise, the hook is called when module to be patched is loaded
 * via the module load notifier.
 */
#define KPATCH_LOAD_HOOK(_fn) \
	static inline kpatch_loadcall_t __loadtest(void) { return _fn; } \
	struct kpatch_load kpatch_load_data __section(.kpatch.hooks.load) = { \
		.fn = _fn, \
		.objname = NULL \
	};

/*
 * KPATCH_UNLOAD_HOOK macro
 *
 * Same as LOAD hook with s/load/unload/
 */
#define KPATCH_UNLOAD_HOOK(_fn) \
	static inline kpatch_unloadcall_t __unloadtest(void) { return _fn; } \
	struct kpatch_unload kpatch_unload_data __section(.kpatch.hooks.unload) = { \
		.fn = _fn, \
		.objname = NULL \
	};

/*
 * KPATCH_FORCE_UNSAFE macro
 *
 * USE WITH EXTREME CAUTION!
 *
 * Allows patch authors to bypass the activeness safety check at patch
 * load time. Do this ONLY IF 1) the patch application will always/likely
 * fail due to the function being on the stack of at least one thread at
 * all times and 2) it is safe for both the original and patched versions
 * of the function to run concurrently.
 *
 * WARNING: Use of this macro will prevent the patch module from ever being
 * rmmod'ed, though it can still be disabled.  This is because the patched
 * function may still be in use after disabling it.  This means you can "kpatch
 * unload" it, but you can't do a "kpatch load" of the same module again later
 * on.
 */
#define KPATCH_FORCE_UNSAFE(_fn) \
	void *__kpatch_force_func_##_fn __section(.kpatch.force) = _fn;

/*
 * KPATCH_PRINTK macro
 *
 * Use this instead of calling printk to avoid unwanted compiler optimizations
 * which cause kpatch-build errors.
 *
 * The printk function is annotated with the __cold attribute, which tells gcc
 * that the function is unlikely to be called.  A side effect of this is that
 * code paths containing calls to printk might also be marked cold, leading to
 * other functions called in those code paths getting moved into .text.unlikely
 * or being uninlined.
 *
 * This macro places printk in its own code path so as not to make the
 * surrounding code path cold.
 */
#define KPATCH_PRINTK(_fmt, ...) \
({ \
	if (jiffies) \
		printk(_fmt, ## __VA_ARGS__); \
})


/*
 * KPATCH_SHADOW_* macros
 *
 * These macros can be used to add new "shadow" fields to existing data
 * structures.
 *
 * For example, to allocate a "newpid" variable associated with an instance of
 * task_struct, and assign it a value of 1000:
 *
 * struct tast_struct *tsk = current;
 * int *newpid;
 * KPATCH_SHADOW_CREATE(tsk, newpid, GFP_KERNEL);
 * if (newpid)
 * 	*newpid = 1000;
 *
 * To retrieve a pointer to the variable:
 *
 * struct tast_struct *tsk = current;
 * int *newpid;
 * KPATCH_SHADOW_GET(tsk, newpid);
 * if (newpid)
 * 	printk("task newpid = %d\n", *newpid); // prints "1000"
 *
 * Notice that the name of the variable specifies the name of the shadow
 * variable.  In this case, the naming of the newpid variable specifies that
 * the shadow variable is named "newpid".
 *
 * To free it:
 *
 * int *newpid;
 * KPATCH_SHADOW_DESTROY(tsk, newpid);
 *
 */

/*
 * KPATCH_SHADOW_CREATE macro
 *
 * Allocates a shadow variable named "var" associated with the obj struct and
 * updates var with a pointer to the new variable.
 *
 * obj: Pointer to the parent struct
 *
 * var: Unassigned pointer for the shadow variable.  The name of the variable
 * specifies the name of the shadow variable.
 *
 * gfp: gfp flags to pass to kmalloc
 */
#define KPATCH_SHADOW_CREATE(obj, var, gfp) ({			\
	typeof(*var) **__shadow;				\
	var = NULL;						\
	__shadow = kpatch_shadow_create(obj, #var, gfp);	\
	if (__shadow) {						\
		*__shadow = kmalloc(sizeof(**__shadow), gfp);	\
		if (!*__shadow)					\
			kpatch_shadow_destroy(obj, #var);	\
		var = *__shadow;				\
	}							\
})

/*
 * KPATCH_SHADOW_DESTROY macro
 *
 * Frees a shadow variable named "var" associated with the obj struct.
 *
 * obj: Pointer to the parent struct
 *
 * var: Unassigned pointer for the shadow variable.  The name of the variable
 * specifies the name of the shadow variable.
 */
#define KPATCH_SHADOW_DESTROY(obj, var) ({			\
	var = kpatch_shadow_destroy(obj, #var);			\
	if (var)						\
		kfree(var);					\
})

/* KPATCH_SHADOW_GET macro
 *
 * Retrieves a shadow variable named "var" associated with the obj struct and
 * updates var with a pointer to the variable.  If the shadow variable doesn't
 * exist, var is set to NULL.
 *
 * obj: Pointer to the parent struct
 *
 * var: Unassigned pointer for the shadow variable.  The name of the variable
 * specifies the name of the shadow variable.
 */
#define KPATCH_SHADOW_GET(obj, var) ({				\
	var = kpatch_shadow_get(obj, #var);			\
})

#endif /* __KPATCH_MACROS_H_ */
