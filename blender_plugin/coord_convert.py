"""
Coordinate system conversion between Blender and DirectX.
Blender: Z-up, right-handed
DirectX: Y-up, left-handed
"""
import struct


def convert_position(x, y, z, scale=1.0):
    """Convert Blender position (X,Y,Z) to DirectX (X,Z,Y) with left-hand flip."""
    return (x * scale, z * scale, y * scale)


def convert_normal(x, y, z):
    """Convert Blender normal to DirectX normal."""
    return (x, z, y)


def convert_uv(u, v):
    """Convert Blender UV to DirectX UV (flip V)."""
    return (u, 1.0 - v)


def convert_quaternion_blender_to_dx(w, x, y, z):
    """
    Convert Blender quaternion (WXYZ, right-hand Z-up)
    to engine quaternion (XYZW, left-hand Y-up).
    """
    # Swap Y and Z axes, negate for handedness change
    dx_x = x
    dx_y = z
    dx_z = y
    dx_w = -w
    return (dx_x, dx_y, dx_z, dx_w)


def convert_matrix4x4_blender_to_dx(blender_matrix, scale=1.0):
    """
    Convert a Blender 4x4 matrix to DirectX row-major format.
    Blender matrix is column-major in Python API.
    Applies axis swap (Y<->Z) and scale.
    Returns flat list of 16 floats (row-major).
    """
    # Blender Matrix is accessed as mat[col][row]
    # Convert to row-major and swap Y/Z axes
    # Swap rows 1 and 2, and columns 1 and 2
    m = [[blender_matrix[col][row] for col in range(4)] for row in range(4)]

    # Swap row 1 and row 2
    m[1], m[2] = m[2], m[1]
    # Swap column 1 and column 2
    for row in range(4):
        m[row][1], m[row][2] = m[row][2], m[row][1]

    # Apply scale to translation
    m[3][0] *= scale
    m[3][1] *= scale
    m[3][2] *= scale

    # Flatten row-major
    return [m[row][col] for row in range(4) for col in range(4)]


def convert_matrix4x3_blender_to_dx(blender_matrix, scale=1.0):
    """
    Convert a Blender 4x4 matrix to DirectX 4x3 format (row-major, 12 floats).
    Only the first 3 columns of a 4x4 row-major matrix.
    """
    mat44 = convert_matrix4x4_blender_to_dx(blender_matrix, scale)
    # 4x3 = rows 0-3, columns 0-2 (skip col 3)
    mat43 = []
    for row in range(4):
        for col in range(3):
            mat43.append(mat44[row * 4 + col])
    return mat43


# ============================================================
# Inverse conversions: DirectX -> Blender
# ============================================================

def convert_position_dx_to_blender(x, y, z, scale=1.0):
    """Convert DirectX position (X,Y,Z) to Blender (X,Z,Y) with scale inverse."""
    inv_scale = 1.0 / scale if scale != 0 else 1.0
    return (x * inv_scale, z * inv_scale, y * inv_scale)


def convert_normal_dx_to_blender(x, y, z):
    """Convert DirectX normal to Blender normal."""
    return (x, z, y)


def convert_uv_dx_to_blender(u, v):
    """Convert DirectX UV to Blender UV (flip V back)."""
    return (u, 1.0 - v)


def convert_quaternion_dx_to_blender(x, y, z, w):
    """
    Convert engine quaternion (XYZW, left-hand Y-up)
    to Blender quaternion (WXYZ, right-hand Z-up).
    """
    bl_w = -w
    bl_x = x
    bl_y = z
    bl_z = y
    return (bl_w, bl_x, bl_y, bl_z)


def convert_matrix4x4_dx_to_blender(flat_mat, scale=1.0):
    """
    Convert a DirectX row-major 4x4 matrix (flat list of 16 floats) to a Blender 4x4 Matrix.
    Reverses axis swap (Y<->Z) and scale.
    Returns a list-of-lists [row][col] suitable for mathutils.Matrix.
    """
    # Reconstruct 4x4 row-major
    m = [[flat_mat[row * 4 + col] for col in range(4)] for row in range(4)]

    # Reverse: swap column 1 and 2
    for row in range(4):
        m[row][1], m[row][2] = m[row][2], m[row][1]
    # Reverse: swap row 1 and 2
    m[1], m[2] = m[2], m[1]

    # Reverse scale on translation (row 3)
    inv_scale = 1.0 / scale if scale != 0 else 1.0
    m[3][0] *= inv_scale
    m[3][1] *= inv_scale
    m[3][2] *= inv_scale

    # Blender Matrix is column-major in API but constructed from rows
    # Return as row-list for Matrix(rows)
    return m


def convert_matrix4x3_dx_to_blender(flat_mat, scale=1.0):
    """
    Convert a DirectX row-major 4x3 matrix (flat list of 12 floats) to Blender 4x4.
    Expands to 4x4 by adding the last column [0,0,0,1].
    Returns list-of-lists for mathutils.Matrix.
    """
    # Expand 4x3 to 4x4
    mat44 = []
    for row in range(4):
        mat44.append(flat_mat[row * 3 + 0])
        mat44.append(flat_mat[row * 3 + 1])
        mat44.append(flat_mat[row * 3 + 2])
        mat44.append(0.0 if row < 3 else 1.0)
    return convert_matrix4x4_dx_to_blender(mat44, scale)
