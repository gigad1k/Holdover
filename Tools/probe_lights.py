import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    def acts():
        return eas.get_all_level_actors()
except AttributeError:
    def acts():
        return unreal.EditorLevelLibrary.get_all_level_actors()

for level in ('/Game/Holdover/Maps/L_RampGreybox', '/Game/FirstPerson/Lvl_FirstPerson'):
    les.load_level(level)
    meshes = 0
    for a in acts():
        cls = a.get_class().get_name()
        if cls == 'StaticMeshActor':
            smc = a.get_component_by_class(unreal.StaticMeshComponent)
            m = smc.get_editor_property('static_mesh') if smc else None
            meshes += 1
            if meshes <= 3:
                unreal.log_error(f'PROBE {level} MESH {a.get_actor_label()} mesh={m and m.get_name()} scale={a.get_actor_scale3d()} loc={a.get_actor_location()}')
        elif cls == 'ExponentialHeightFog':
            f = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
            try:
                d = f.get_editor_property('fog_density')
            except Exception:
                d = '?'
            unreal.log_error(f'PROBE {level} FOG density={d}')
        elif cls == 'DirectionalLight':
            unreal.log_error(f'PROBE {level} SUN rot={a.get_actor_rotation()}')
    unreal.log_error(f'PROBE {level} total_staticmeshes={meshes}')
