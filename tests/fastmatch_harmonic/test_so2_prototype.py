import json
import math
import random
import unittest
from dataclasses import replace
from so2_prototype import Config, descriptor, distance, match

def polygon(vertices, per_edge=20):
    result=[]
    for a,b in zip(vertices,vertices[1:]+vertices[:1]):
        result.extend([(a[0]+(b[0]-a[0])*j/per_edge,
                        a[1]+(b[1]-a[1])*j/per_edge)
                       for j in range(per_edge)])
    return result

def shape(name):
    if name in ("circle","ellipse"):
        return [(80*math.cos(2*math.pi*j/256),
                 (80 if name=="circle" else 45)*math.sin(2*math.pi*j/256))
                for j in range(256)]
    if name=="square":
        return polygon([(-60,-60),(60,-60),(60,60),(-60,60)])
    if name=="rectangle":
        return polygon([(-80,-40),(80,-40),(80,40),(-80,40)])
    if name=="triangle":
        return polygon([(80*math.cos(2*math.pi*j/3),
                         80*math.sin(2*math.pi*j/3)) for j in range(3)])
    return polygon([(-80,-45),(70,-45),(85,-15),(35,-5),(35,60),
                    (-15,60),(-25,10),(-80,15)])

def transform(points, angle, scale=1.0):
    c,s=math.cos(math.radians(angle)),math.sin(math.radians(angle))
    return [(scale*(c*x-s*y)+143,scale*(s*x+c*y)-57) for x,y in points]

def angular_error(a,b):
    return abs((a-b+180)%360-180)

class SO2Tests(unittest.TestCase):
    def test_rotation_scale_translation_and_start(self):
        points=shape("asymmetric")
        for method in ["dft","efd"]:
            reference=descriptor(points,method=method,closed=True)
            for angle in range(0,360,15):
                observed=transform(points,angle,1.7)
                observed=observed[13:]+observed[:13]
                result=match(reference,descriptor(observed,method=method,closed=True))
                self.assertTrue(result["succeeded"],(method,angle,result))
                self.assertLess(min(angular_error(p["angle_deg"],angle)
                                    for p in result["pose_hypotheses"]),0.05)
                self.assertLess(result["invariant_distance"],0.001)
                self.assertAlmostEqual(result["pose_hypotheses"][0]["scale"],1.7,places=3)

    def test_winding_and_density(self):
        for method in ["dft","efd"]:
            points=shape("asymmetric")
            ref=descriptor(points,method=method,closed=True)
            rev=descriptor(list(reversed(points)),method=method,closed=True)
            self.assertLess(distance(ref,rev),1e-10)
            dense=polygon(points,3)
            self.assertLess(distance(ref,descriptor(dense,method=method,closed=True)),1e-9)

    def test_symmetry_hypotheses(self):
        for name,order in [("ellipse",2),("rectangle",2),("square",4),("triangle",3)]:
            result=match(descriptor(shape(name),closed=True,method="efd"),
                         descriptor(transform(shape(name),37),closed=True,method="efd"))
            self.assertTrue(result["succeeded"],(name,result))
            self.assertEqual(result["symmetry_order"],order)
            self.assertEqual(len(result["pose_hypotheses"]),order,(name,result))
            self.assertLess(min(angular_error(p["angle_deg"],37)
                                for p in result["pose_hypotheses"]),0.01)

    def test_circle_has_no_invented_angle(self):
        result=match(descriptor(shape("circle"),closed=True),
                     descriptor(transform(shape("circle"),71),closed=True))
        self.assertFalse(result["succeeded"])
        self.assertEqual(result["pose_hypotheses"],[])
        self.assertEqual(result["status"],"ORIENTATION_UNOBSERVABLE")

    def test_reflection_not_conflated_with_winding(self):
        points=shape("asymmetric")
        result=match(descriptor(points,closed=True),
                     descriptor([(-x,y) for x,y in points],closed=True))
        self.assertFalse(result["succeeded"])
        self.assertEqual(result["fallback_reason"],"SHAPE_OR_REFLECTION_MISMATCH")

    def test_invalid_topology_and_numeric_input(self):
        for flags,reason in [
            (dict(closed=False),"OPEN_CONTOUR"),
            (dict(closed=True,complete=False),"PARTIAL_CONTOUR"),
            (dict(closed=True,holes=1),"UNSUPPORTED_TOPOLOGY"),
            (dict(closed=True,components=2),"UNSUPPORTED_TOPOLOGY")]:
            with self.assertRaisesRegex(ValueError,reason):
                descriptor(shape("asymmetric"),**flags)
        points=shape("asymmetric")
        points[0]=(float("nan"),0)
        with self.assertRaisesRegex(ValueError,"NONFINITE"):
            descriptor(points,closed=True)
        with self.assertRaisesRegex(ValueError,"INSUFFICIENT"):
            descriptor([(0,0),(10,0),(0,10)],closed=True)
        with self.assertRaisesRegex(ValueError,"INVALID_MAX_ORDER"):
            descriptor(shape("square"),Config(max_order=128),closed=True)
        with self.assertRaisesRegex(ValueError,"PERIMETER_TOO_SMALL"):
            descriptor([(x/100,y/100) for x,y in shape("square")],closed=True)

    def test_dft_analytic_efd_parity(self):
        for name in ["asymmetric","square","ellipse","triangle"]:
            points=shape(name)
            a=descriptor(points,closed=True)
            b=descriptor(points,method="efd",closed=True)
            self.assertLess(distance(a,b),0.001)
            self.assertLess(max(abs(x-y) for x,y in zip(a["coefficients"],b["coefficients"])),0.001)

    def test_wrong_shape_reject_and_bounded_noise(self):
        points=shape("asymmetric")
        rng=random.Random(20260930)
        noisy=[(x+rng.uniform(-.1,.1),y+rng.uniform(-.1,.1)) for x,y in transform(points,73)]
        self.assertTrue(match(descriptor(points,closed=True),
                              descriptor(noisy,closed=True))["succeeded"])
        self.assertFalse(match(descriptor(points,closed=True),
                               descriptor(shape("ellipse"),closed=True))["succeeded"])

    def test_audit_never_changes_seed_or_candidate_set(self):
        result=match(descriptor(shape("rectangle"),closed=True),
                     descriptor(transform(shape("rectangle"),24),closed=True))
        self.assertFalse(result["used_for_seed"])
        self.assertFalse(result["used_for_prefilter"])
        self.assertFalse(result["measurement_evidence"])

if __name__=="__main__":
    unittest.main()
