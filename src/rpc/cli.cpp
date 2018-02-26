#include <fc/rpc/cli.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>

#ifndef WIN32
#include <unistd.h>
#endif

#ifdef HAVE_EDITLINE
# include "editline.h"
# ifdef WIN32
#  include <io.h>
# endif
#endif

namespace fc { namespace rpc {

static std::vector<std::string>& cli_commands()
{
   static std::vector<std::string>* cmds = new std::vector<std::string>();
   return *cmds;
}

cli::~cli()
{
   if( _run_complete.valid() )
   {
      stop();
   }
}

variant cli::send_call( api_id_type api_id, string method_name, variants args /* = variants() */ )
{
   FC_ASSERT(false);
}

variant cli::send_callback( uint64_t callback_id, variants args /* = variants() */ )
{
   FC_ASSERT(false);
}

void cli::send_notice( uint64_t callback_id, variants args /* = variants() */ )
{
   FC_ASSERT(false);
}

void cli::start()
{
   cli_commands() = get_method_names(0);
   _run_complete = fc::async( [&](){ run(); } );
}

void cli::stop()
{
   _run_complete.cancel();
   _run_complete.wait();
}

void cli::wait()
{
   _run_complete.wait();
}

void cli::format_result( const string& method, std::function<string(variant,const variants&)> formatter)
{
   _result_formatters[method] = formatter;
}

void cli::set_prompt( const string& prompt )
{
   _prompt = prompt;
}

void cli::run()
{
   while( !_run_complete.canceled() )
   {
      try
      {
         std::string line;
         try
         {
            getline( _prompt.c_str(), line );
         }
         catch ( const fc::eof_exception& e )
         {
            break;
         }
         std::cout << line << "\n";
         line += char(EOF);
         fc::variants args = fc::json::variants_from_string(line);;
         if( args.size() == 0 )
            continue;

         const string& method = args[0].get_string();

         auto result = receive_call( 0, method, variants( args.begin()+1,args.end() ) );
         auto itr = _result_formatters.find( method );
         if( itr == _result_formatters.end() )
         {
            std::cout << fc::json::to_pretty_string( result ) << "\n";
         }
         else
            std::cout << itr->second( result, args ) << "\n";
      }
      catch ( const fc::exception& e )
      {
         std::cout << e.to_detail_string() << "\n";
      }
   }
}

/****
 * @brief loop through list of commands, attempting to find a match
 * @param token what the user typed
 * @param match
 * @returns the full name of the command or NULL if no matches found
 */
static char *my_rl_complete(char *token, int *match)
{
    int matchlen = 0;
    int count = 0;
    const char* method_name;

    auto& cmd = cli_commands();
    for (auto it : cmd) {
    	int partlen = strlen (token); /* Part of token */

    	if (!strncmp (it.c_str(), token, partlen)) {
    		method_name = it.c_str();
    		matchlen = partlen;
    		count ++;
    	}
    }

    if (count == 1) {
    	*match = 1;
    	return strdup (method_name + matchlen);
    }

    return NULL;
}

/***
 * @brief return an array of matching commands
 * @param token the incoming text
 * @param av the resultant array of possible matches
 * @returns the number of matches
 */
static int cli_completion(char *token, char ***av)
{
    int num, total = 0;
    char **copy;

    auto& cmd = cli_commands();
    num = cmd.size();

    copy = (char **) malloc (num * sizeof(char *));
    for (auto it : cmd) {
    	if (!strncmp (it.c_str(), token, strlen (token))) {
    		copy[total] = strdup ( it.c_str() );
    		total ++;
    	}
    }
    *av = copy;

    return total;
}

/***
 * @brief Read input from the user
 * @param prompt the prompt to display
 * @param line what the user typed
 */
void cli::getline( const fc::string& prompt, fc::string& line)
{
   // getting file descriptor for C++ streams is near impossible
   // so we just assume it's the same as the C stream...
#ifdef HAVE_EDITLINE
#ifndef WIN32   
   if( isatty( fileno( stdin ) ) )
#else
   // it's implied by
   // https://msdn.microsoft.com/en-us/library/f4s0ddew.aspx
   // that this is the proper way to do this on Windows, but I have
   // no access to a Windows compiler and thus,
   // no idea if this actually works
   if( _isatty( _fileno( stdin ) ) )
#endif
   {
      //rl_attempted_completion_function = cli_completion;
	   rl_set_complete_func(my_rl_complete);
	   rl_set_list_possib_func(cli_completion);

      static fc::thread getline_thread("getline");
      getline_thread.async( [&](){
         char* line_read = nullptr;
         std::cout.flush(); //readline doesn't use cin, so we must manually flush _out
         line_read = readline(prompt.c_str());
         if( line_read == nullptr )
            FC_THROW_EXCEPTION( fc::eof_exception, "" );
         if( *line_read )
            add_history(line_read);
         line = line_read;
         free(line_read);
      }).wait();
   }
   else
#endif
   {
      std::cout << prompt;
      // sync_call( cin_thread, [&](){ std::getline( *input_stream, line ); }, "getline");
      fc::getline( fc::cin, line );
      return;
   }
}

} } // namespace fc::rpc
