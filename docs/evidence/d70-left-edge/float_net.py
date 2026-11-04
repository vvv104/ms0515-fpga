"""A net driven by nothing but N LS inputs (diode-input LS: R1 from Vcc to an
internal node B, a Schottky diode from B down to the pin, B clamped by the
two base-emitter junctions it drives).  D70 (74LS257) lets go of the net at
a level; its off-state leakage pulls down (worst case).  Integrate the pin
voltage, report when every input reads high and where the net settles."""
import math

VT = 0.0259


def node_b(vpin, vcc, r1, is_d, is_q):
    """Solve (Vcc-VB)/R1 = Id(VB-Vpin) + Iq(VB) for VB by bisection;
    return VB, the diode (pin) current, the clamp (base) current."""
    lo, hi = -1.0, vcc
    for _ in range(80):
        vb = (lo + hi) / 2
        idd = is_d * (math.exp((vb - vpin) / (1.05 * VT)) - 1)
        iq = is_q * math.exp(vb / (2 * 1.0 * VT))
        if (vcc - vb) / r1 > idd + iq:
            lo = vb
        else:
            hi = vb
    return vb, idd, iq


def run(v0, n, c, r1, vcc, leak, t_end=3e-6, dt=0.05e-9):
    # Schottky: 0.25 mA at 0.35 V; clamp: two junctions take 0.25 mA at VB = 1.40 V
    is_d = 0.25e-3 / math.exp(0.35 / (1.05 * VT))
    is_q = 0.25e-3 / math.exp(1.40 / (2 * VT))
    v, t, t_high, i_il = v0, 0.0, None, None
    while t < t_end:
        vb, idd, iq = node_b(v, vcc, r1, is_d, is_q)
        if i_il is None:
            i_il = idd
        high = iq > idd  # the clamp takes most of R1's current: the gate sees 1
        if high and t_high is None:
            t_high = t
        if not high and t_high is not None:
            raise SystemExit("fell back to 0 at %g s" % t)
        v += (n * idd - leak) / c * dt
        t += dt
    return t_high, v, i_il, vb


print("release from LOW (0.35 V): time until all inputs read 1, settled level")
print(" N   C,pF  R1,k  Vcc   leak,uA   I_IL,mA/input  t_high,ns  V_settle  VB")
for n in (4,):
    for c in (30e-12, 50e-12, 80e-12):
        for r1 in (11e3, 15e3, 20e3):
            for vcc in (4.75, 5.25):
                th, vs, iil, vb = run(0.35, n, c, r1, vcc, 20e-6)
                print(f" {n}  {c*1e12:4.0f}  {r1/1e3:4.0f}  {vcc:4.2f}  20        "
                      f"{iil*1e3:5.3f}          {th*1e9:6.1f}     {vs:5.3f}    {vb:5.3f}")

print()
print("release from HIGH (3.4 V), worst leakage 20 uA (D70 off) + 4 x 20 uA (IIH):")
for c in (30e-12, 80e-12):
    th, vs, iil, vb = run(3.4, 4, c, 15e3, 5.0, 100e-6, t_end=5e-6)
    print(f" C {c*1e12:3.0f} pF: after 5 us the net is at {vs:5.3f} V, reads 1 throughout")
print()
print("current needed to pull the net low: 4 x I_IL, ~1 mA - a 1-2 kOhm path to ground")
