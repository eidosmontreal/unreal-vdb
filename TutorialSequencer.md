# Sequencer Tutorial

Based on popular demand, here is a quick step-by-step tutorial to correctly setup a sequence of OpenVDB files in Unreal's `Sequencer`, how to use it with the path-tracer, and how to render a high quality movie with it.

## Context

I'm really happy to see that the VFX and the offline rendering community has shown a lot of interest for this plugin, 
and particularly in the pathtracer mode using the `Sequencer` because it allows baking high-quality images and sequences
of images with realtime tools and realtime preview. 

As a reminder, only `VdbResearchActors` are compatible with Unreal's pathtracer. As the name implies, they are 
experimental and were initially created for programmers that can change and adapt the HLSL shaders as they see fit. 
This mode does not use Unreal materials, it uses a hard-coded shader with parameters exposed directly in the `VdbComponent`.  

 
## Prerequisites

You need to [install and enable](README.md#Installation) this plugin.

Import any sequence in the editor, please refer to [this documentation](README.md) if you don't know how to.

In this tutorial, I will use [Embergen's Gasoline explosion](https://jangafx.com/software/embergen/download/free-vdb-animations/).
I'm only importing the `Density` and `Temperature` grids, we will not use the `Flames` grid.

## Step by step tutorial

### Step 1. Create new Actor

The first step is to create a new VdbResearchActor. 
![CreateVdbReseachActor](Resources/TutorialSequencer/CreateVdbReseachActor.png)

### Step 2. Set correct `VdbVolumes` in `VdbComponent`

In our case, 
* `Vdb Density` should link to `embergen_gasoline_explosion_a_seq_density`
* `Vdb Temperature` should link to `embergen_gasoline_explosion_a_seq_temperature`.
 
![SetVolumes](Resources/TutorialSequencer/SetVolumes.png)

In general cases, `Vdb Density` is mandatory but `Vdb Temperature` is **optional**. 
Temperature will not be sampled if not defined. For example, Clouds don't use Temperature information. 

### Step 3. Check your VDB Sequence

Most `VdbVolumeSequences` will start with an empty or very small volume on the first frame. 
I recommend offseting your sequence so that you can actually see your volume. 
It is configurable in the `Playback` panel, with the `Offset` parameter.

![OffsetSequence](Resources/TutorialSequencer/OffsetSequence.png)

### Step 4. Check Volume orientation

Once you can see your volume, you will probably notice some issues with the volume orientation.
Unreal uses a different coordinate systems than most other Digital Content Creator tools. 
Your imported VDB files will probably be rotated or flipped, 
use the `Actor Transform` to fix orientation.   

### Step 5. Enable Pathtracer

Enable your pathtracer, [following Epic's instructions](https://docs.unrealengine.com/4.27/en-US/RenderingAndGraphics/RayTracing/PathTracer/).

Before:
![PTBefore](Resources/TutorialSequencer/ActivatePathtracing.png)

After:
![PTAfter](Resources/TutorialSequencer/PathtracingActivated.png)

### Step 6. Make it look good

Depending on your volume type (smoke, fog, cloud etc.), tweak the rendering options available in the component,
 mostly under `Principled Volume`.

![Params](Resources/TutorialSequencer/TweakParameters.png)

### Step 7. Create and use a Sequence

It is now time to create a sequence to be able to control the animation, using the [Sequencer](https://docs.unrealengine.com/4.27/en-US/AnimatingObjects/Sequencer/Overview/).

![CreateSeq](Resources/TutorialSequencer/CreateSequence.png)

We can now add (`Track` in Unreal terms) our `VdbResearchActor` in the `Sequencer`.

![AddActorToSeq](Resources/TutorialSequencer/AddActorToSequence.png)

### Step 8. Add VDB sequence in Sequencer

This is probably the only tricky part of the tutorial, make sure to follow the next steps closely.

There are two ways to add `VDB sequences` in the `Sequencer`.

#### Density only animations

When adding a `Track` on your `VdbResearchActor`, you can simply select `Tracks/Vdb Sequence`, 
as explained [here](README.md#sequencer_density).

![AddSingleSeq](Resources/TutorialSequencer/AddSingleSequence.png)

#### Density and Temperature animations

When using a VDB sequence with `Temperature`, you will need to control each `VdbVolumeSequences` separately 
by tracking both `DensitySequenceComponent` and `TemperatureSequenceComponent`.

You can then add a `Tracks/Vdb Sequence` on each tracked component.

| Add `Density Sequence Component` tracking | Add sequencer tracking with a `Vdb Sequence` |
| ----------- | ----------- |
| ![AdDensity](Resources/TutorialSequencer/AddMultipleSquences_AddDensityComponent.png) | ![AddSeqToDensity](Resources/TutorialSequencer/AddSeqToDensity.png) |

| Add `Temperature Sequence Component` tracking | Add sequencer tracking with a `Vdb Sequence` |
| ----------- | ----------- |
| ![Addtemp](Resources/TutorialSequencer/AddMultipleSquences_AddTemperatureComponent.png) | ![AddSeqToTemp](Resources/TutorialSequencer/AddSeqToTemperature.png) |

     
Warning: My `Sequencer` knowledge and experience is very limited. If you have some constructive feedback or suggestions, 
feel free to [reach out to me](https://twitter.com/LambertTibo).


### Step 9. Use the Sequencer

Your `Sequencer` should now be working, and you can use the timeline to browse through the animation.

![Sequencer](Resources/TutorialSequencer/SequencerWorking.png)

Warning: There is currently an issue where changing frames in the sequencer doesn't reset the pathtracer view. 
I've informed Epic and [they fixed it](https://github.com/EpicGames/UnrealEngine/commit/23dba4db228ec6f6df7eec632ebb5fb15dba47f1), but it's not available in the current release yet, hopefully in the next one.

  
### Step 10. Add cinematic camera

To finalize the `Sequence`, you need to add a `Cinematic Camera` (choose the one you prefer). 
 
![CreateCam](Resources/TutorialSequencer/CreateCinematicCamera.png)

Place the camera correctly in the world so that the VDB sequence fits in the frame. 
I recommend using the `Pilot` option to easily move the Camera and see the results live.

![PilotCam](Resources/TutorialSequencer/PilotCamera.png)

Then add (`Track`) it to your `Sequence`:

![AddCAmSeq](Resources/TutorialSequencer/AddCameraToSequencer.png)

You are now ready to use and render the `Sequence`.
 
![RadyRender](Resources/TutorialSequencer/ReadyToRender.png)


### Step 11. Using the Movie Render Queue (MRQ)

You can now render and bake your `Sequence`, for a trailer, a cinematic, a short film etc.

As always, I recommend reading the [Epic documentation](https://docs.unrealengine.com/4.26/en-US/AnimatingObjects/Sequencer/Workflow/RenderAndExport/HighQualityMediaExport/), especially the 
[section that combines the MRQ with the pathtracer](https://docs.unrealengine.com/4.27/en-US/RenderingAndGraphics/RayTracing/PathTracer/#path-tracedrendersusingmovierenderqueue).

Once the plugin is activated, open the `Movie Render Queue` panel, and select your `Sequence` to be rendered:

![MRQ](Resources/TutorialSequencer/OpenMRQ.png)
![MRQSeq](Resources/TutorialSequencer/SelectSequence.png)

For reference, I use these basic settings to bake our example `Sequence`. 
![SetupMRQ](Resources/TutorialSequencer/SetupMRQ.png)


## Conclusion

This is it, you can now render any sequence of OpenVDB (or NanoVDB) files in Unreal, in combination with the pathtracer.

I hope this tutorial was clear enough and will help you set up your own animations easily. 
I'm always happy to see what the community comes up with, so feel free to share your results on social media !



