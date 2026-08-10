// Mock audio renderer for headless runs (determinism_runner). Implements the
// full AudioRenderer interface as no-ops so the audio message queue can be
// drained without opening an audio device (GitHub Actions runners have none).

#ifndef _HPP_MOCK_AUDIORENDERER
#define _HPP_MOCK_AUDIORENDERER

#include "interface_audiorenderer.hpp"

namespace blunted {

  class MockAudioRenderer : public AudioRenderer {

    public:
      MockAudioRenderer() : nextSoundBufferID(1) {}
      virtual ~MockAudioRenderer() {}

      // init & exit
      virtual bool CreateContext() { return true; }
      virtual void Exit() {}

      virtual int CreateAudioSoundBuffer(const WavData *wavData) { return nextSoundBufferID++; }
      virtual void DeleteAudioSoundBuffer(int audioSoundBufferID) {}
      virtual void PlayAudioSoundBuffer(int audioSoundBufferID) {}

      virtual void SetListenerParameters(const Vector3 &position, const Vector3 &velocity, const Quaternion &orientation) {}

      virtual void SetSourceParameter(int audioSoundBufferID, e_AudioRenderer_SourceParameter parameter, float value) {}

      void operator()() {
        bool quit = false;
        while (!quit) {
          bool isMessage;
          boost::intrusive_ptr<Command> message = messageQueue.WaitForMessage(isMessage, 1);
          if (isMessage) {
            if (!message->Handle(this)) quit = true;
            message.reset();
          }
        }
      }

    protected:
      int nextSoundBufferID;

  };

}

#endif
