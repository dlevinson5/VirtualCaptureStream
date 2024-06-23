# DirectShow Virtual Capture Sink/Source Filter

This is a DirectShow Graph Filter that allows capturing a media stream in one process and making it available in a second process as a virtual capture source. The idea of this filter
is to encapsulate the details of the capture source in one process and allow a secondary process to consume the stream. 

This project requires Visual Studio 2019 

I developed this filter many years ago and is available for general curiosity. 

The magic in this filter is mainly in how it uses a Windows file pipes to transmit the media samples across two processes. A capture source in one process attaches to a *VirtualSink* filter that using Windows system pipes to transmit the necessary pin descripters and media samples to a secondard process. The secondary process uses the *VirtualStreamSource* filter to attach to the pipe and recreate the source output pins making it appear is a normal capture source. The filters  use DirectShow buffers to maintain throughput of samples as they are processed through the grapgh. Windows pipe throughout is very fast and was never a concern at the time for handling SD or HD streams. This filter took me a while to get working and my description is not doing justice to how cool this filter is IMHO. 

Here is a general workflow. 

[Process 1] Capture Source -> VirtualSink -> Windows Pipes(s) -> [Process 2] VirtualSource -> (Capture Graph)


