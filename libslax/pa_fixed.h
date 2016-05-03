/*
 * Copyright (c) 2016, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Phil Shafer <phil@>, April 2016
 */

#ifndef LIBSLAX_PA_FIXED_H
#define LIBSLAX_PA_FIXED_H

/**
 * Paged arrays are fixed size arrays, allocated piecemeal
 * to reduce their initial memory impact.
 *
 * Glossary:
 * atom = smallest addressable unit.  You are making an arrray of
 *     these atoms.  Atom size is determined by the user by
 *     in the atom_size parameter.
 * page = a set of atoms.  The number of atom per page is given in
 *     the shift parameter: a page hold 1<<shift atoms.
 * page_arrray = an array of atoms, split into pages to avoid
 *     forcing a large memory footprint for an array that _might_
 *     be large.
 * base = the array of pointers to pages
 *
 * The paged array works like a page table; part of the identifier
 * selects a page of values, while the remainder selects an atom off
 * that page.  Note that it's an atom array, not a byte array.  The
 * caller sets the bit shift (page size) and the number and size of
 * the atoms, which are recorded in the base pa_fixed_t structure.
 * Since the caller knows these values, they can either let us get
 * them from the struct or pass them directly to the inline functions
 * (e.g. pa_atom_addr_direct) for better performance.
 *
 * Be aware that since we're layering our "atoms" on top of mmap's
 * "atoms", we need to keep our thinking clear in terms of whose atoms
 * are whose.  Our "pages" are their "atoms".  We then divide each
 * page into our fixed-sized atoms.
 */

/*
 * These functions are designed to point into memory mapped addresses,
 * but can be used without one, since I wanted them to be useful in
 * more broad circumstances.  The macros in this section allow us
 * to make both sets of functions off one source file.
 */

#ifdef PA_NO_MMAP
#define PA_FIXED_MMAP_FIELD_DECLS /* Nothing */

typedef void *pa_fixed_page_entry_t;

#else /* PA_NO_MMAP */

#define PA_FIXED_MMAP_FIELD_DECLS pa_mmap_t *pf_mmap

typedef pa_atom_t pa_fixed_page_entry_t;

#endif /* PA_NO_MMAP */

typedef struct pa_fixed_info_s {
    pa_shift_t pfi_shift;	/* Bits to shift to select the page */
    uint8_t pfi_padding;	/* Padding this by hand */
    uint16_t pfi_atom_size;	/* Size of each atom */
    pa_atom_t pfi_max_atoms;	/* Max number of atoms */
    pa_atom_t pfi_free;		/* First atom that is free */
    pa_atom_t pfi_base;		/* Offset of page table base (in mmap atoms) */
} pa_fixed_info_t;

typedef struct pa_fixed_s {
    PA_FIXED_MMAP_FIELD_DECLS;	   /* Mmap overhead declarations */
    pa_fixed_info_t pf_info_block; /* Simple block (not used for mmap) */
    pa_fixed_info_t *pf_infop;	   /* Pointer to real block */
    pa_fixed_page_entry_t *pf_base; /* Base of page table */
} pa_fixed_t;

/* Simplification macros, so we don't need to think about pf_infop */
#define pf_shift	pf_infop->pfi_shift
#define pf_atom_size	pf_infop->pfi_atom_size
#define pf_max_atoms	pf_infop->pfi_max_atoms
#define pf_free		pf_infop->pfi_free

#ifdef PA_NO_MMAP

static inline void
pa_fixed_set_base (pa_fixed_t *pfp, pa_atom_t atom UNUSED,
		   pa_fixed_page_entry_t *base)
{
    pfp->pf_base = base;
}

static inline void *
pa_fixed_page_get (pa_fixed_t *pfp, pa_page_t page)
{
    return pfp->pf_base[page];
}

static inline void
pa_fixed_page_set (pa_fixed_t *pfp, pa_page_t page, void *addr)
{
    pfp->pf_base[page] = addr;
}

#else /* PA_NO_MMAP */

static inline void
pa_fixed_base_set (pa_fixed_t *pfp, pa_atom_t atom,
		   pa_fixed_page_entry_t *base)
{
    pfp->pf_base = base;
    pfp->pf_infop->pfi_base = atom;
}

/*
 * Our pages are directly allocated from mmap, so we can use the mmap
 * address function to get a real address.
 */
static inline void *
pa_fixed_page_get (pa_fixed_t *pfp, pa_page_t page)
{
    if (pfp->pf_base[page] == PA_NULL_ATOM)
	return NULL;

    return pa_mmap_addr(pfp->pf_mmap, pfp->pf_base[page]);
}

static inline void
pa_fixed_page_set (pa_fixed_t *pfp, pa_page_t page,
		   pa_atom_t atom, void *addr UNUSED)
{
    pfp->pf_base[page] = atom;
}

#endif /* PA_NO_MMAP */

/*
 * Return the address of an atom in a paged table array.  This
 * version takes all information as arguments, so one can truly
 * get inline access.  Suitable for wrapping in other inlines
 * with constants for any specific pa_fixed_t's parameters.
 */
static inline void *
pa_fixed_atom_addr_direct (pa_fixed_t *pfp, uint8_t shift, uint16_t atom_size,
			   uint32_t max_atoms, pa_atom_t atom)
{
    uint8_t *addr;
    if (atom == 0 || atom >= max_atoms)
	return NULL;

    addr = pa_fixed_page_get(pfp, atom >> shift);
    if (addr == NULL)
	return NULL;

    /*
     * We have the base address of the page, and need to find the
     * address of our atom inside the page.
     */
    return &addr[(atom & ((1 << shift) - 1)) * atom_size];
}

/*
 * Return the address of an atom in a paged table array using the info
 * recorded in our header.
 */
static inline void *
pa_fixed_atom_addr (pa_fixed_t *pfp, uint32_t atom)
{
    if (pfp->pf_base == NULL)
	return NULL;

    return pa_fixed_atom_addr_direct(pfp, pfp->pf_shift, pfp->pf_atom_size,
			       pfp->pf_max_atoms, atom);
}

void
pa_fixed_alloc_setup_page (pa_fixed_t *pfp, pa_atom_t atom);

/*
 * Allocate a new atom, returning the atom number
 */
static inline pa_atom_t
pa_fixed_alloc_atom (pa_fixed_t *pfp)
{
    /* free == PA_NULL_ATOM -> nothing available */
    if (pfp->pf_free == PA_NULL_ATOM || pfp->pf_base == NULL)
	return PA_NULL_ATOM;

    /* Take the next atom off the free list and return it */
    pa_atom_t atom = pfp->pf_free;
    void *addr = pa_fixed_atom_addr(pfp, atom);
    if (addr == NULL) {
	/*
	 * The atom number was on the free list but we got NULL as the
	 * address.  The only way this happens is when the page is
	 * freshly unallocated.  So we force it to be allocated and
	 * re-fetch the address, which may fail again if the underlaying
	 * allocator (mmap) can't allocate memory.
	 */
	pa_fixed_alloc_setup_page(pfp, atom);
	addr = pa_fixed_atom_addr(pfp, atom);
    }

    /*
     * Fetch the next free atom from the start of this atom.  If we
     * failed, we don't want to change the free atom, since it might
     * be a transient memory issue.
     */
    if (addr)
	pfp->pf_free = *(pa_atom_t *) addr; /* Fetch next item on free list */
    else
	pa_alloc_failed(__FUNCTION__);

    return atom;
}

/*
 * Put a fixed atom on our free list
 */
static inline void
pa_fixed_free_atom (pa_fixed_t *pfp, pa_atom_t atom)
{
    if (atom == PA_NULL_ATOM)
	return;

    pa_atom_t *addr = pa_fixed_atom_addr(pfp, atom);
    if (addr == NULL)
	return;

    /* Add the atom to the front of the free list */
    *addr = pfp->pf_free;
    pfp->pf_free = atom;
}

void
pa_fixed_init_from_block (pa_fixed_t *pfp, void *base,
			  pa_fixed_info_t *infop);

void
pa_fixed_init (pa_mmap_t *pmp, pa_fixed_t *pfp, pa_shift_t shift,
	       uint16_t atom_size, uint32_t max_atoms);

pa_fixed_t *
pa_fixed_setup (pa_mmap_t *pmp, pa_fixed_info_t *pfip, pa_shift_t shift,
		uint16_t atom_size, uint32_t max_atoms);

pa_fixed_t *
pa_fixed_open (pa_mmap_t *pmp, const char *name, pa_shift_t shift,
	       uint16_t atom_size, uint32_t max_atoms);

void
pa_fixed_close (pa_fixed_t *pfp);

#endif /* LIBSLAX_PA_FIXED_H */