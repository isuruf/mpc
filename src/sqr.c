/* mpc_sqr -- Square a complex number.

Copyright (C) 2002, 2005, 2008, 2009, 2010, 2011, 2012 INRIA

This file is part of GNU MPC.

GNU MPC is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

GNU MPC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this program. If not, see http://www.gnu.org/licenses/ .
*/

#include <stdio.h>    /* for MPC_ASSERT */
#include "mpc-impl.h"

#define mpz_add_si(z,x,y) do { \
   if (y >= 0) \
      mpz_add_ui (z, x, (long int) y); \
   else \
      mpz_sub_ui (z, x, (long int) (-y)); \
   } while (0);

static int
mpfr_fmma (mpfr_ptr z, mpfr_srcptr a, mpfr_srcptr b, mpfr_srcptr c,
           mpfr_srcptr d, int sign, mpfr_rnd_t rnd)
{
   /* Computes z = ab+cd if sign >= 0, or z = ab-cd if sign < 0.
      Assumes that a, b, c, d are finite and non-zero; so any multiplication
      of two of them yielding an infinity is an overflow, and a
      multiplication yielding 0 is an underflow.
      Assumes further that z is distinct from a, b, c, d. */

   int inex;
   mpfr_t u, v;

   /* u=a*b, v=sign*c*d exactly */
   mpfr_init2 (u, mpfr_get_prec (a) + mpfr_get_prec (b));
   mpfr_init2 (v, mpfr_get_prec (c) + mpfr_get_prec (d));
   mpfr_mul (u, a, b, GMP_RNDN);
   mpfr_mul (v, c, d, GMP_RNDN);
   if (sign < 0)
      mpfr_neg (v, v, GMP_RNDN);

   /* tentatively compute z as u+v; here we need z to be distinct
      from a, b, c, d to not lose the latter */
   inex = mpfr_add (z, u, v, rnd);

   if (mpfr_inf_p (z)) {
      /* replace by "correctly rounded overflow" */
      mpfr_set_si (z, (mpfr_signbit (z) ? -1 : 1), GMP_RNDN);
      inex = mpfr_mul_2ui (z, z, mpfr_get_emax (), rnd);
   }
   else if (mpfr_inf_p (u) || mpfr_inf_p (v)
            || mpfr_zero_p (u) || mpfr_zero_p (v)) {
      /* There is at least one over- or underflow.
         Redo the computations with mpz_t exponents */
      mpfr_exp_t ea, eb, ec, ed;
      mpz_t eu, ev;
         /* cheat to work around the const qualifiers */

      /* Normalise the input by shifting and keep track of the shifts in
         the exponents of u and v */
      ea = mpfr_get_exp (a);
      eb = mpfr_get_exp (b);
      ec = mpfr_get_exp (c);
      ed = mpfr_get_exp (d);

      mpfr_set_exp ((mpfr_ptr) a, (mpfr_prec_t) 0);
      mpfr_set_exp ((mpfr_ptr) b, (mpfr_prec_t) 0);
      mpfr_set_exp ((mpfr_ptr) c, (mpfr_prec_t) 0);
      mpfr_set_exp ((mpfr_ptr) d, (mpfr_prec_t) 0);

      mpz_init (eu);
      mpz_init (ev);
      mpz_set_si (eu, (long int) ea);
      mpz_add_si (eu, eu, (long int) eb);
      mpz_set_si (ev, (long int) ec);
      mpz_add_si (ev, ev, (long int) ed);

      /* recompute u and v and move exponents to eu and ev */
      mpfr_mul (u, a, b, GMP_RNDN);
      /* exponent of u is non-positive */
      mpz_sub_ui (eu, eu, (unsigned long int) (-mpfr_get_exp (u)));
      mpfr_set_exp (u, (mpfr_prec_t) 0);
      mpfr_mul (v, c, d, GMP_RNDN);
      if (sign < 0)
         mpfr_neg (v, v, GMP_RNDN);
      mpz_sub_ui (ev, ev, (unsigned long int) (-mpfr_get_exp (v)));
      mpfr_set_exp (v, (mpfr_prec_t) 0);

      if (mpfr_nan_p (z)) {
         mpfr_exp_t emax = mpfr_get_emax ();
         int overflow;
         /* We have a = ma * 2^ea with 1/2 <= |ma| < 1 and ea <= emax, and
            analogously for b. So eu <= 2*emax, and eu > emax since we have
            an overflow. The same holds for ev. Shift u and v by as much as
            possible so that one of them has exponent emax and the
            remaining exponents in eu and ev are the same. Then carry out
            the addition. Shifting u and v prevents an underflow. */
         if (mpz_cmp (eu, ev) >= 0) {
            mpfr_set_exp (u, emax);
            mpz_sub_ui (eu, eu, (long int) emax);
            mpz_sub (ev, ev, eu);
            mpfr_set_exp (v, (mpfr_exp_t) mpz_get_ui (ev));
               /* remaining common exponent is now in eu */
         }
         else {
            mpfr_set_exp (v, emax);
            mpz_sub_ui (ev, ev, (long int) emax);
            mpz_sub (eu, eu, ev);
            mpfr_set_exp (u, (mpfr_exp_t) mpz_get_ui (eu));
            mpz_set (eu, ev);
               /* remaining common exponent is now also in eu */
         }
         inex = mpfr_add (z, u, v, rnd);
            /* Result is finite since u and v have different signs. */
         overflow = mpfr_mul_2ui (z, z, mpz_get_ui (eu), rnd);
         if (overflow)
            inex = overflow;
      }
      else {
         int underflow;
         /* Addition of two zeroes with same sign. We have a = ma * 2^ea
            with 1/2 <= |ma| < 1 and ea >= emin and similarly for b.
            So 2*emin < 2*emin+1 <= eu < emin < 0, and analogously for v. */
         mpfr_exp_t emin = mpfr_get_emin ();
         if (mpz_cmp (eu, ev) <= 0) {
            mpfr_set_exp (u, emin);
            mpz_add_ui (eu, eu, (unsigned long int) (-emin));
            mpz_sub (ev, ev, eu);
            mpfr_set_exp (v, (mpfr_exp_t) mpz_get_si (ev));
         }
         else {
            mpfr_set_exp (v, emin);
            mpz_add_ui (ev, ev, (unsigned long int) (-emin));
            mpz_sub (eu, eu, ev);
            mpfr_set_exp (u, (mpfr_exp_t) mpz_get_si (eu));
            mpz_set (eu, ev);
         }
         inex = mpfr_add (z, u, v, rnd);
         mpz_neg (eu, eu);
         underflow = mpfr_div_2ui (z, z, mpz_get_ui (eu), rnd);
         if (underflow)
            inex = underflow;
      }

      mpz_clear (eu);
      mpz_clear (ev);

      mpfr_set_exp ((mpfr_ptr) a, ea);
      mpfr_set_exp ((mpfr_ptr) b, eb);
      mpfr_set_exp ((mpfr_ptr) c, ec);
      mpfr_set_exp ((mpfr_ptr) d, ed);
         /* works also when some of a, b, c, d are not all distinct */
   }

   mpfr_clear (u);
   mpfr_clear (v);

   return inex;
}

int
mpc_sqr (mpc_ptr rop, mpc_srcptr op, mpc_rnd_t rnd)
{
   int ok;
   mpfr_t u, v;
   mpfr_t x;
      /* temporary variable to hold the real part of op,
         needed in the case rop==op */
   mpfr_prec_t prec;
   int inex_re, inex_im, inexact;
   mpfr_exp_t emin, emax;
   int saved_underflow;

   /* special values: NaN and infinities */
   if (!mpc_fin_p (op)) {
      if (mpfr_nan_p (mpc_realref (op)) || mpfr_nan_p (mpc_imagref (op))) {
         mpfr_set_nan (mpc_realref (rop));
         mpfr_set_nan (mpc_imagref (rop));
      }
      else if (mpfr_inf_p (mpc_realref (op))) {
         if (mpfr_inf_p (mpc_imagref (op))) {
            mpfr_set_inf (mpc_imagref (rop),
                          MPFR_SIGN (mpc_realref (op)) * MPFR_SIGN (mpc_imagref (op)));
            mpfr_set_nan (mpc_realref (rop));
         }
         else {
            if (mpfr_zero_p (mpc_imagref (op)))
               mpfr_set_nan (mpc_imagref (rop));
            else
               mpfr_set_inf (mpc_imagref (rop),
                             MPFR_SIGN (mpc_realref (op)) * MPFR_SIGN (mpc_imagref (op)));
            mpfr_set_inf (mpc_realref (rop), +1);
         }
      }
      else /* IM(op) is infinity, RE(op) is not */ {
         if (mpfr_zero_p (mpc_realref (op)))
            mpfr_set_nan (mpc_imagref (rop));
         else
            mpfr_set_inf (mpc_imagref (rop),
                          MPFR_SIGN (mpc_realref (op)) * MPFR_SIGN (mpc_imagref (op)));
         mpfr_set_inf (mpc_realref (rop), -1);
      }
      return MPC_INEX (0, 0); /* exact */
   }

   prec = MPC_MAX_PREC(rop);

   /* Check for real resp. purely imaginary number */
   if (mpfr_zero_p (mpc_imagref(op))) {
      int same_sign = mpfr_signbit (mpc_realref (op)) == mpfr_signbit (mpc_imagref (op));
      inex_re = mpfr_sqr (mpc_realref(rop), mpc_realref(op), MPC_RND_RE(rnd));
      inex_im = mpfr_set_ui (mpc_imagref(rop), 0ul, GMP_RNDN);
      if (!same_sign)
        mpc_conj (rop, rop, MPC_RNDNN);
      return MPC_INEX(inex_re, inex_im);
   }
   if (mpfr_zero_p (mpc_realref(op))) {
      int same_sign = mpfr_signbit (mpc_realref (op)) == mpfr_signbit (mpc_imagref (op));
      inex_re = -mpfr_sqr (mpc_realref(rop), mpc_imagref(op), INV_RND (MPC_RND_RE(rnd)));
      mpfr_neg (mpc_realref(rop), mpc_realref(rop), GMP_RNDN);
      inex_im = mpfr_set_ui (mpc_imagref(rop), 0ul, GMP_RNDN);
      if (!same_sign)
        mpc_conj (rop, rop, MPC_RNDNN);
      return MPC_INEX(inex_re, inex_im);
   }

   if (rop == op)
   {
      mpfr_init2 (x, MPC_PREC_RE (op));
      mpfr_set (x, op->re, GMP_RNDN);
   }
   else
      x [0] = op->re [0];
   /* From here on, use x instead of op->re and safely overwrite rop->re. */

   /* Compute real part of result. */
   if (SAFE_ABS (mpfr_exp_t,
                 mpfr_get_exp (mpc_realref (op)) - mpfr_get_exp (mpc_imagref (op)))
       > (mpfr_exp_t) MPC_MAX_PREC (op) / 2) {
      /* If the real and imaginary parts of the argument have very different
         exponents, it is not reasonable to use Karatsuba squaring; compute
         exactly with the standard formulae instead, even if this means an
         additional multiplication. Using the approach copied from mul, over-
         and underflows are also handled correctly. */

      inex_re = mpfr_fmma (rop->re, x, x, op->im, op->im, -1,
                           MPC_RND_RE (rnd));

   }
   else {
      /* Karatsuba squaring */
      mpfr_init (u);
      mpfr_init (v);

      emax = mpfr_get_emax ();
      emin = mpfr_get_emin ();

      do
      {
         prec += mpc_ceil_log2 (prec) + 5;

         mpfr_set_prec (u, prec);
         mpfr_set_prec (v, prec);

         /* Let op = x + iy. We need u = x+y and v = x-y, rounded away.      */
         /* The error is bounded above by 1 ulp.                             */
         /* We first let inexact be 1 if the real part is not computed       */
         /* exactly and determine the sign later.                            */
         inexact =    ROUND_AWAY (mpfr_add (u, x, mpc_imagref (op), MPFR_RNDA), u)
                  | ROUND_AWAY (mpfr_sub (v, x, mpc_imagref (op), MPFR_RNDA), v);

         /* compute the real part as u*v, rounded away                    */
         /* determine also the sign of inex_re                            */
         if (mpfr_sgn (u) == 0 || mpfr_sgn (v) == 0)
         {
            /* as we have rounded away, the result is exact */
            mpfr_set_ui (mpc_realref (rop), 0, GMP_RNDN);
            inex_re = 0;
            ok = 1;
         }
         else if (mpfr_sgn (u) * mpfr_sgn (v) > 0)
         {
            inexact |= mpfr_mul (u, u, v, GMP_RNDU); /* error 5 */
            /* checks that no overflow nor underflow occurs: since u*v > 0
               and we round up, an overflow will give +Inf, but an underflow
               will give 0.5*2^emin */
            MPC_ASSERT (mpfr_get_exp (u) != emin);
            if (mpfr_inf_p (u))
               {
               /* let mpc_realref(rop) be a "correctly rounded overflow" */
               inex_re = mpfr_set_ui_2exp (mpc_realref (rop), 1, emax, MPC_RND_RE (rnd));
               break;
               }
            ok = (!inexact) | mpfr_can_round (u, prec - 3, GMP_RNDU, GMP_RNDZ,
                  MPC_PREC_RE (rop) + (MPC_RND_RE (rnd) == GMP_RNDN));
            if (ok)
               {
               inex_re = mpfr_set (mpc_realref (rop), u, MPC_RND_RE (rnd));
               if (inex_re == 0)
                  /* remember that u was already rounded */
                  inex_re = inexact;
               }
         }
         else
         {
            mpfr_rnd_t rnd_re = MPC_RND_RE (rnd);
            inexact |= mpfr_mul (u, u, v, GMP_RNDD); /* error 5 */
            /* if an overflow occurs: since u*v < 0 and we round down,
               the result is -Inf or -MAXDBL */
            if (mpfr_inf_p (u))
               {
               /* replace by "correctly rounded overflow" */
               mpfr_set_si (u, -1, GMP_RNDN);
               mpfr_mul_2ui (u, u, mpfr_get_emax (), rnd_re);
               ok = 1;
               }
            else
               {
               /* if an underflow happens (i.e., u = -0.5*2^emin since we round
                  away from zero), the result will be an underflow */
               if (mpfr_get_exp (u) == emin)
                  {
                     if (rnd_re == GMP_RNDZ || rnd_re == GMP_RNDN ||
                        rnd_re == GMP_RNDU)
                     {
                        mpfr_set_ui (mpc_realref (rop), 0, rnd_re);
                        inex_re = 1;
                     }
                     else /* round down or away from zero */ {
                     mpfr_set (mpc_realref (rop), u, rnd_re);
                     inex_re = -1;
                     }
                     break;
                  }
               ok = (!inexact) | mpfr_can_round (u, prec - 3, GMP_RNDD,
                  GMP_RNDZ, MPC_PREC_RE (rop) + (MPC_RND_RE (rnd) == GMP_RNDN));
               }
            if (ok)
               {
               inex_re = mpfr_set (mpc_realref (rop), u, MPC_RND_RE (rnd));
               if (inex_re == 0)
                  inex_re = inexact;
               }
         }
      }
      while (!ok);

      mpfr_clear (u);
      mpfr_clear (v);
   }

   saved_underflow = mpfr_underflow_p ();
   mpfr_clear_underflow ();
   inex_im = mpfr_mul (rop->im, x, op->im, MPC_RND_IM (rnd));
   if (!mpfr_underflow_p ())
      inex_im |= mpfr_mul_2exp (rop->im, rop->im, 1, MPC_RND_IM (rnd));
      /* We must not multiply by 2 if rop->im has been set to the smallest
         representable number. */
   if (saved_underflow)
      mpfr_set_underflow ();

   if (rop == op)
      mpfr_clear (x);

   return MPC_INEX (inex_re, inex_im);
}
