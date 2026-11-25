"""Original procedural mannequin geometry with Blender skin weights."""
from math import cos, pi, sin

import bpy
from mathutils import Matrix, Vector

from motion import DIGITS, SOCKET, SOCKET_OFFSET, UPPER_LENGTH, Z


def build_mannequin(rig):
    materials = []
    for name, rgb, metallic, rough in (
        ('Graphite textile', (.043,.065,.076), .12, .51),
        ('Jade polymer', (.075,.27,.25), .28, .39),
        ('Warm porcelain', (.49,.36,.25), .04, .49),
        ('Seam tape', (.024,.037,.043), .17, .44),
        ('Porcelain face', (.57,.42,.30), .04, .48),
        ('Oxide trim', (.52,.23,.09), .30, .43),
        ('Dark hair', (.082,.056,.040), 0, .62),
        ('Face shadows', (.16,.092,.068), 0, .65),
        ('Eye detail', (.38,.40,.36), 0, .56),
    ):
        m = bpy.data.materials.new(name)
        m.diffuse_color = (*rgb, 1)
        m.use_nodes = True
        shader = m.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (*rgb, 1)
        shader.inputs['Metallic'].default_value = metallic
        shader.inputs['Roughness'].default_value = rough
        materials.append(m)

    vertices, faces, surface, weights = [], [], [], []

    def rings(sections, transform, material, steps=24):
        start = len(vertices)
        for center, rx, rz, weight in sections:
            for j in range(steps):
                angle = j * 2*pi/steps
                vertices.append(transform(Vector(center) + Vector((rx*cos(angle),0,rz*sin(angle)))))
                weights.append(weight)
        for k in range(len(sections)-1):
            for j in range(steps):
                a, b = start + k*steps + j, start + k*steps + (j+1)%steps
                faces.append((a,b,b+steps,a+steps))
                surface.append(material[k] if isinstance(material,list) else material)
        faces.extend((tuple(start+j for j in reversed(range(steps))),
                      tuple(len(vertices)-steps+j for j in range(steps))))
        surface.extend([material[0], material[-1]] if isinstance(material,list) else [material]*2)

    def body(rows, material):
        rings([((0,z,y),w,d,weight) for z,y,w,d,weight in rows],
              lambda v: Vector((v.x,-v.z,v.y)), material, 40)

    def sleeve(bone, rows, material, steps=24):
        item = rig.data.bones[bone]
        rings([((x,y,z),w,d,{bone:1}) for y,w,d,x,z in rows],
              lambda v:item.matrix_local@v, material, steps)

    def oval(bone, center, scale, material):
        c, s = Vector(center), Vector(scale)
        sleeve(bone, [(c.y+s.y*t, max(.015,(1-t*t)**.5)*s.x, max(.015,(1-t*t)**.5)*s.z,c.x,c.z)
                      for t in (-1,-.94,-.78,-.50,-.23,0,.23,.50,.78,.94,1)], material)

    def right_arm():
        """One continuous skin over the shoulder, hinge and distributed forearm roll."""
        upper = rig.data.bones['UpperArm.R']
        forearm = rig.data.bones['Forearm.R']
        upper_space, forearm_space = upper.matrix_local.copy(), forearm.matrix_local.copy()
        x_axis = upper_space.to_quaternion() @ Vector((1,0,0))
        upper_depth = upper_space.to_quaternion() @ Z
        forearm_depth = forearm_space.to_quaternion() @ Z

        def arm_space(point):
            y = point.y - UPPER_LENGTH
            center = upper_space @ Vector((0,point.y,0)) if y <= 0 else forearm_space @ Vector((0,y,0))
            mix = min(1, max(0, (y + .044) / .088))
            depth = upper_depth.lerp(forearm_depth, mix).normalized()
            return center + point.x*x_axis + point.z*depth

        # Ring height, width and depth, elbow blend, roll blend, clavicle blend.
        profile = [
            (-.056,.009,.015,0,0,.80), (-.037,.047,.048,0,0,.50),
            (-.010,.062,.064,0,0,.26), (.025,.071,.072,0,0,.10),
            (.074,.068,.063,0,0,0), (.124,.060,.054,0,0,0),
            (.149,.054,.049,0,0,0), (.159,.052,.047,0,0,0),
            (.195,.045,.041,0,0,0), (.240,.040,.037,0,0,0),
            (.258,.039,.035,.07,0,0), (.277,.039,.037,.27,0,0),
            (.295,.040,.039,.52,0,0), (.313,.041,.039,.77,0,0),
            (.335,.043,.039,.97,.08,0), (.357,.044,.038,1,.26,0),
            (.397,.040,.035,1,.50,0), (.435,.037,.033,1,.74,0),
            (.465,.033,.028,1,.88,0), (.488,.032,.026,1,.96,0),
            (.493,.035,.028,1,.97,0), (.518,.029,.023,1,1,0),
            (.531,.028,.022,1,1,0), (.550,.025,.020,1,1,0),
        ]
        sections = []
        for y, width, depth, elbow, roll, clavicle in profile:
            weights = {
                'Clavicle.R': clavicle,
                'UpperArm.R': (1-clavicle)*(1-elbow),
                'Forearm.R': elbow*(1-roll),
                'ForearmRoll.R': elbow*roll,
            }
            sections.append(((0,y,0), width, depth, {name:value for name,value in weights.items() if value}))
        material = [1]*6 + [3] + [2]*13 + [5,3,2]
        rings(sections, arm_space, material, 32)

    p, s, c = {'Pelvis':1}, {'Spine':1}, {'Chest':1}
    body([
        (.883,0,.126,.085,p), (.911,0,.164,.103,p), (.957,0,.178,.107,p),
        (1.005,0,.161,.099,p), (1.022,0,.157,.100,s),
        (1.092,-.004,.153,.103,s), (1.171,-.005,.159,.108,{'Spine':.75,'Chest':.25}),
        (1.242,-.010,.178,.112,{'Spine':.15,'Chest':.85}), (1.316,-.010,.193,.116,c),
        (1.401,-.010,.202,.108,c), (1.446,-.009,.182,.093,c), (1.472,-.010,.118,.079,c),
        (1.486,-.010,.067,.066,c),
    ], [0,0,0,3,0,0,1,1,1,1,1,3])

    oval('Neck',(0,.066,0),(.063,.104,.062),2)
    # Head and facial planes use original simple profiles.
    sleeve('Head',[(y,w,d,0,z) for y,w,d,z in (
        (-.012,.026,.036,-.025),(.011,.053,.058,-.025),(.044,.067,.070,-.016),
        (.071,.077,.080,-.010),(.095,.080,.081,-.007),(.120,.084,.088,-.001),
        (.156,.086,.089,.005),(.189,.077,.080,.008),(.217,.052,.059,.009),
        (.228,.011,.030,.009))],[4,4,4,4,4,4,6,6,6],36)
    oval('Head',(0,.084,-.094),(.016,.030,.021),4)
    oval('Head',(0,.043,-.078),(.029,.0035,.003),7)
    for i in (-1,1):
        oval('Head',(i*.079,.088,.005),(.014,.025,.019),2)
        oval('Head',(i*.034,.103,-.079),(.019,.005,.008),7)
        oval('Head',(i*.034,.103,-.084),(.011,.003,.003),8)
        oval('Head',(i*.037,.116,-.079),(.023,.004,.005),6)

    for side in ('R','L'):
        k = '.'+side
        if side == 'R':
            right_arm()
        else:
            bone = 'UpperArm'+k
            oval(bone,(0,.017,0),(.073,.090,.073),1 if side=='R' else 0)
            sleeve(bone,[(-.02,.049,.053,0,0),(.028,.070,.071,0,0),
                         (.081,.068,.064,0,0),(.126,.060,.054,0,0),
                         (.15,.053,.050,0,0),(.16,.052,.048,0,0),
                         (.195,.045,.042,0,0),(.252,.038,.038,0,0),
                         (.293,.036,.034,0,0)],[1,1,1,1,3,2,2,2])
            oval(bone,(0,.289,0),(.036,.039,.034),2)
            bone = 'Forearm'+k
            sleeve(bone,[(-.023,.032,.032,0,0),(0,.039,.036,0,0),
                         (.050,.045,.042,0,0),(.104,.042,.037,0,0),
                         (.171,.035,.031,0,0),(.194,.032,.026,0,0),
                         (.198,.035,.028,0,0),(.227,.029,.023,0,0),
                         (.255,.025,.021,0,0)],[2,2,2,2,2,5 if side=='R' else 3,3,2])
        sleeve('Hand'+k,[(-.016,.022,.019,0,0),(.005,.025,.022,0,0),
                         (.027,.034,.026,0,.003),(.054,.043,.025,0,.001),
                         (.077,.044,.020,0,0),(.095,.037,.014,.002,0)],2)
        for j, name in enumerate(DIGITS):
            for n in range(1,4):
                digit = f'{name}{n}'+k
                length = rig.data.bones[digit].length
                radius = (.014 if name=='Thumb' else .0125-abs(j-2)*.001)*(.87**(n-1))
                oval(digit,(0,length*.43,0),(radius,length*.68,radius*.87),2)
                if n == 3:
                    oval(digit,(0,length*.60,-radius*.73),(radius*.69,length*.29,radius*.22),4)
        bone = 'Thigh'+k
        sleeve(bone,[(-.068,.06,.071,0,0),(-.012,.093,.101,0,0),
                     (.061,.096,.100,0,0),(.133,.088,.091,0,0),
                     (.222,.074,.075,0,0),(.351,.059,.063,0,0),
                     (.426,.058,.060,0,0)],[0,0,0,0,0,3])
        bone = 'Shin'+k
        sleeve(bone,[(-.033,.054,.057,0,0),(.009,.057,.062,0,0),
                     (.067,.061,.073,0,-.007),(.115,.063,.076,0,-.008),
                     (.217,.051,.059,0,-.003),(.249,.046,.051,0,0),
                     (.258,.052,.055,0,0),(.328,.048,.051,0,0),
                     (.404,.041,.045,0,0)],[0,0,0,0,0,5,3,3])
        bone = 'Foot'+k
        sleeve(bone,[(-.075,.033,.037,0,0),(-.055,.052,.058,0,0),
                     (-.012,.057,.062,0,0),(.060,.054,.057,0,.005),
                     (.111,.056,.047,0,.004),(.155,.058,.035,0,.007),
                     (.199,.049,.027,0,.012),(.221,.024,.020,0,.016)],3)
        sleeve(bone,[(-.074,.035,.011,0,-.034),(-.052,.054,.013,0,-.047),
                     (.013,.059,.013,0,-.049),(.103,.061,.013,0,-.038),
                     (.181,.056,.012,0,-.019),(.221,.031,.010,0,-.003)],0)

    mesh = bpy.data.meshes.new("Original mannequin surfaces")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    model = bpy.data.objects.new("Mannequin", mesh)
    bpy.context.collection.objects.link(model)
    for m in materials:
        mesh.materials.append(m)
    for face, material in zip(mesh.polygons, surface):
        face.material_index = material
        face.use_smooth = True
    # Consistent outward normals, including the arbitrary joint profiles.
    import bmesh
    bmesh_data = bmesh.new()
    bmesh_data.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bmesh_data, faces=bmesh_data.faces)
    bmesh_data.to_mesh(mesh)
    bmesh_data.free()
    groups = {bone.name:model.vertex_groups.new(name=bone.name) for bone in rig.data.bones}
    for index, weight in enumerate(weights):
        for bone, amount in weight.items():
            groups[bone].add([index], amount, 'REPLACE')
    model.parent = rig
    modifier = model.modifiers.new('Mannequin skin','ARMATURE')
    modifier.object = rig

    socket = bpy.data.objects.new(SOCKET, None)
    bpy.context.collection.objects.link(socket)
    item = rig.data.bones['Hand.R']
    socket.parent = rig
    socket.parent_type = 'BONE'
    socket.parent_bone = item.name
    socket.matrix_parent_inverse = Matrix.Translation((0, -item.length, 0))
    socket.location = SOCKET_OFFSET
    socket.empty_display_size = .045
    return model, socket
