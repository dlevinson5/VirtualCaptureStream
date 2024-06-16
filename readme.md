# DirectShow Virtual Capture Sink/Source Filter

This is a DirectShow Graph Filter that allows capturing a media stream in one process and making it available in a second process as a virtual capture source. The idea of this filter
is to encapsulate the details of the capture source in one process and allow a secondary process to consume the stream. 
 
I developed this filter many years ago and is available for general curiosity. 

The magic in this filter is mainly in how it uses a system pipe to transmit the media samples across two processes. When the capture source is attached to the VirtualStreamSink the filter will pipeline out pin descriptors via a "VirtualSink1" pipe. the VirtualSourceSink filter will connect with this pipe and use the descriptors to establish the output pins. The filter then uses the standard windows pipe and DirectShow buffering to communicate between processes. At the time this served its purpose and probably could have used some code cleanup. 

