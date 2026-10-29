import math

class SaurianLegIK:
    def __init__(self, h_ht=52.0, v_ht=40.0, z_ht=35.0, femur_len=110.0, tibia_len=120.0):
        # 3D Hip-to-Thigh offsets (in mm)
        self.H_ht = h_ht        # 5.2 cm radial outward offset
        self.V_ht = v_ht        # 4.0 cm vertical drop offset
        self.Z_ht = z_ht        # 3.5 cm lateral shaft offset (use -35.0 for opposite-side legs)
        
        # Link lengths (hypotenuse center-to-center in mm)
        self.L2 = femur_len     # Thigh horn to calf horn
        self.L3 = tibia_len     # Calf horn to foot tip

    def solve(self, x, y, z):
        """
        x: Forward/Back reach from hip pivot (mm)
        y: Lateral outward reach from hip pivot (mm)
        z: Vertical depth below hip pivot (mm, positive = down)
        """
        # 1. Top-Down View: Account for Z_ht (35mm) lateral offset
        r_sq = x**2 + y**2
        if r_sq < self.Z_ht**2:
            print("Target inside deadzone of Z_ht offset!")
            return None
            
        r_plane = math.sqrt(r_sq - self.Z_ht**2)
        
        # Hip Yaw angle with Z_ht angular compensation
        theta_hip = math.atan2(y, x) - math.atan2(self.Z_ht, r_plane)
        
        # 2. Subtract H_ht (52mm) and V_ht (40mm) to shift origin to the Thigh Horn
        d_horiz = r_plane - self.H_ht
        d_vert = z - self.V_ht
        
        # Direct 2D distance from thigh horn to foot tip
        D = math.sqrt(d_horiz**2 + d_vert**2)
        
        # Clamp to physical leg reach limits
        max_reach = (self.L2 + self.L3) * 0.999
        min_reach = abs(self.L2 - self.L3) * 1.001
        D = max(min_reach, min(D, max_reach))
        
        # 3. Law of Cosines for Thigh and Calf
        cos_thigh = (self.L2**2 + D**2 - self.L3**2) / (2 * self.L2 * D)
        thigh_inner = math.acos(max(-1.0, min(1.0, cos_thigh)))
        
        reach_angle = math.atan2(d_horiz, d_vert)
        theta_thigh = reach_angle + thigh_inner
        
        cos_calf = (self.L2**2 + self.L3**2 - D**2) / (2 * self.L2 * self.L3)
        theta_calf = -(math.pi - math.acos(max(-1.0, min(1.0, cos_calf))))
        
        return theta_hip, theta_thigh, theta_calf