#pragma once
#include "Shader.h"
#include "Decoder.h"
#include "DecodedFrame.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <queue>
#include <sstream>

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/video_decoder.h"
#include "ppapi/utility/completion_callback_factory.h"

#include "pnacl_player_assert.h"

namespace PnaclPlayer
{
	class pnacl_player : public pp::Instance, public pp::Graphics3DClient
	{
	public:
		pnacl_player(PP_Instance instance, pp::Module* module);
		virtual ~pnacl_player();

		// pp::Instance implementation.
		virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip_ignored);

		// pp::Init implementation lets us access startup arguments
		virtual bool Init(uint32_t argc, const char * argn[], const char * argv[]);

		// pp::Graphics3DClient implementation.
		virtual void Graphics3DContextLost()
		{
			// TODO(vrk/fischman): Properly reset after a lost graphics context.  In particular need to delete context_ and re-create textures.
			// Probably have to recreate the decoder from scratch, because old textures can still be outstanding in the decoder!
			assert(false && "Unexpectedly lost graphics context");
		}
		void ReceiveDecodedPicture(DecodedFrame frame);

		void DelayedPaint(int32_t result, DecodedFrame frame);

		void PaintPicture(DecodedFrame frame);

		/// <summary>
		/// Handler for messages coming in from the browser via postMessage().  The argument "var_message" can be any pp:Var type; for example int, string, Array, or Dictionary. Please see the pp:Var documentation for more details.
		/// </summary>
		/// <param name="var_message">The message posted by the browser.</param>
		virtual void HandleMessage(const pp::Var& var_message);

#pragma region Error Logging
		// Log an error to the developer console and stderr by creating a temporary
		// object of this type and streaming to it.  Example usage:
		// LogError(this).s() << "Hello world: " << 42;
		class LogError
		{
		public:
			LogError(pnacl_player* instance) : instance_(instance) {}
			~LogError()
			{
				const std::string& msg = stream_.str();
				instance_->console_if_->Log(instance_->pp_instance(), PP_LOGLEVEL_ERROR, pp::Var(msg).pp_var());
				std::cerr << msg << std::endl;
			}
			// Impl note: it would have been nicer to have LogError derive from
			// std::ostringstream so that it can be streamed to directly, but lookup
			// rules turn streamed string literals to hex pointers on output.
			std::ostringstream& s() { return stream_; }

		private:
			pnacl_player* instance_;
			std::ostringstream stream_;
		};
#pragma endregion

		/// <summary>
		/// Send a string to the browser
		/// </summary>
		void PostString(std::string message);

	private:

		void InitializeDecoders();
#pragma region Declare GL-related functions
		// GL-related functions.
		void InitGL();
		void CreateGLObjects();
		void Create2DProgramOnce();
		void CreateRectangleARBProgramOnce();
		void CreateExternalOESProgramOnce();
		Shader CreateProgram(const char* vertex_shader, const char* fragment_shader);
		void CreateShader(GLuint program, GLenum type, const char* source, int size);
		void PaintNextPicture();
		void PaintFinished(int32_t result);
#pragma endregion

		pp::Size plugin_size_;
		bool is_painting_;
		int hwaccel_;
		// When decode outpaces render, we queue up decoded pictures for later painting.
		std::queue<DecodedFrame> pending_pictures_;

		int num_frames_rendered_;
		PP_TimeTicks first_frame_delivered_ticks_;
		PP_TimeTicks last_swap_request_ticks_;
		PP_TimeTicks swap_ticks_;
		pp::CompletionCallbackFactory<pnacl_player> callback_factory_;

		// Unowned pointers.
		const PPB_Console* console_if_;
		const PPB_Core* core_if_;
		const PPB_OpenGLES2* gles2_if_;

		// Owned data.
		/// <summary>
		/// The pp::Graphics3D context
		/// </summary>
		pp::Graphics3D* context_;
		Decoder* video_decoder_;
		long long nextFrameTimestamp;

#pragma region Shader Stuff
		// Shader program to draw GL_TEXTURE_2D target.
		Shader shader_2d_;
		// Shader program to draw GL_TEXTURE_RECTANGLE_ARB target.
		Shader shader_rectangle_arb_;
		// Shader program to draw GL_TEXTURE_EXTERNAL_OES target.
		Shader shader_external_oes_;
#pragma endregion
	};
}