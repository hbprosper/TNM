// -*- C++ -*-
//
// Package:    PhysicsTools
// Class:      FunctionMember
// 
/**\class FunctionMember FunctionMember.cc 
   PhysicsTools/TheNtupleMaker/src/FunctionMember.cc

   Description: model simple or compound methods

   Implementation:
   Common sense and a sense of beauty.
*/
//
// Original Author:  Harrison B. Prosper
//         Created:  Tue Dec  8 15:40:26 CET 2009
//                   Wed Oct 20 HBP - go back to logging all warnings
// $Id: FunctionMember.cc,v 1.7 2010/10/20 16:08:01 prosper Exp $
//-----------------------------------------------------------------------------
#include <Python.h>
#include <boost/python.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <string>
#include <stdlib.h>
//-----------------------------------------------------------------------------
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "PhysicsTools/TheNtupleMaker/interface/FunctionMember.h"
#include "PhysicsTools/TheNtupleMaker/interface/colors.h"
#include "PhysicsTools/TheNtupleMaker/interface/CurrentEvent.h"
//-----------------------------------------------------------------------------
using namespace std;
using namespace ROOT::Reflex;
//-----------------------------------------------------------------------------
static bool DBFunctionMember = getenv("DBFunctionMember")>0 ? true : false; 
static bool FirstCallToFM=true;

RunToTypeMap FunctionMember::donotcall = RunToTypeMap();

FunctionMember::FunctionMember()
  : classname_(""),
    expression_("")
{}


FunctionMember::~FunctionMember() 
{ 
  for(unsigned depth=0; depth < fd_.size(); ++depth)
    {
      FunctionDescriptor& fd = fd_[depth]; // NB: get an alias NOT a copy!

      if ( DBFunctionMember )
        cout << "       - deallocate memory for: " 
             << BLUE 
             << fd.classname << "::" << fd.expression
             << DEFAULT_COLOR
             << endl;
      fd.rtype.Deallocate(fd.robject.Address());

      for(unsigned j=0; j < fd.values.size(); ++j) delete fd.values[j];
    }
}

// Model an instantiable method using the Reflex tools

FunctionMember::FunctionMember(std::string classname, 
                               std::string expression)
  : classname_(classname),
    expression_(expression)
{
  if ( FirstCallToFM )
    {
      FirstCallToFM = false;
      cout << endl << "\t==> Using FunctionMember to call methods <=="
           << endl << endl;
    }

  if ( DBFunctionMember )
    cout << endl 
         << "BEGIN FunctionMember - " 
         << classname_ + "::" << expression_ << endl;

  if ( classname_ == "" ) 
    throw cms::Exception("InvalidClassname") << "null classname" << endl;

  if ( expression_ == "" ) 
    throw cms::Exception("InvalidExpression") << "null expression" << endl;

  //--------------------------------------------------------------------------
  // Split method into its parts. Surely, even for CMS, a maximum 
  // indirection depth of 20 is sufficient!
  //--------------------------------------------------------------------------
  int maxDepth = 20;

  // This flag has to be true at the end of this routine, otherwise something
  // is wrong.

  bool done = false;

  //--------------------------------------------------------------------------
  // Note:
  // classname  - type name of parent object
  // expression - method/datamember of parent object to be invoked
  // retname    - type name of returned object or data member
  // fd         - a vector of function descriptors
  //--------------------------------------------------------------------------

  for(int depth=0; depth < maxDepth; ++depth)
    {
      if ( classname == "" )
        throw cms::Exception("InvalidClassname") << "null classname" << endl;

      if ( expression == "" )
        throw cms::Exception("InvalidExpression") << "null expression" << endl;

      // Allocate a descriptor. We use the descriptor to cache everything 
      // that is needed to render the calling of the method as efficient 
      // as possible.

      if ( DBFunctionMember )
        cout << BLUE
             << "       - allocate function descriptor " << depth
             << DEFAULT_COLOR << endl; 
      
      fd_.push_back( FunctionDescriptor() );
      FunctionDescriptor& fd = fd_[depth]; // NB: get an alias NOT a copy!
      
      string retname(""); // Name of returned type
      fd.classname   = classname;
      fd.expression  = expression;
      fd.otype       = Type::ByName(fd.classname); // Model parent object
      if ( fd.otype.Name() == "" )
        throw cms::Exception("NullClassName") 
          << "FunctionMember sadly cannot find type of the object whose method"
          << " is to be called"
          << endl;
      
      fd.datamember  = false;
      fd.simple      = false;
      fd.pointer     = false;
      fd.reference   = false;
      fd.smartpointer= false;
      fd.isAvailable = false;
      fd.isNull      = false;

      // If method is compound, split it in two and set "expression"
      // to the first part and "expr2" to the remainder

      string delim(""); // delimeter between expression and expr2
      string expr2("");
      if ( rfx::isCompoundMethod(expression, delim) ) 
        rfx::bisplit(expression, fd.expression, expr2, delim);

      if ( DBFunctionMember )
        cout << "       - method/datamember " << ": " 
             << fd.classname << "::" 
             << BLUE << fd.expression << DEFAULT_COLOR << endl
             << "       - join (" << delim << ")"<< endl;

      // Determine whether this is a function or data member
  
      boost::regex dregex("^[a-zA-Z_]+[a-zA-Z0-9_:]*[(]");
      boost::smatch dmatch;

      fd.datamember = ! boost::regex_search(fd.expression, dmatch, dregex);

      if ( fd.datamember )    
        {
          //-------------------------------------------------------------------
          // This seems to be a data member
          //-------------------------------------------------------------------
          if ( DBFunctionMember )
            cout << "       - is " << RED << "data member" 
                 << DEFAULT_COLOR << endl;

          // Get a model of it

          fd.method = rfx::getDataMember(fd.classname, fd.expression);

          // Fall on sword if we did not find a valid data member

          if ( !rfx::memberValid(fd.method) )
            throw cms::Exception("getDataMemberFailure")
              << " can't decode data member: " 
              << fd.classname << "::" 
              << fd.expression << endl;

          // We have a valid data member. Get type allowing for 
          // the possibility that values can be returned 
          // by value, pointer or reference.
          // 1. by value     - a copy of the object is returned
          // 2. by pointer   - a variable (the pointer) containing the 
          //                   address of the object is returned
          // 3. by reference - the object itself is returned

          fd.rtype = fd.method.TypeOf();     // Model type of data member
          if ( fd.rtype.IsFundamental() )
            fd.simple = true;

          if ( fd.rtype.IsPointer() )
            fd.pointer = true;

          else if ( fd.rtype.IsReference() )
            fd.reference = true;

          // Get type name of data member
          // Note: for data members, the rtype variable
          // isn't the final type in the sense that it
          // could still include the "*" or "&" appended to the
          // type name. However, for methods rtype is the final
          // type.
            
          fd.rname = fd.rtype.Name(SCOPED+FINAL);
          if ( DBFunctionMember )
            cout << "       - datamember type: " 
                 << BLUE
                 << retname
                 << DEFAULT_COLOR << endl;

          if ( fd.pointer || fd.reference )
            // remove "*" or "&" at end of name
            fd.rname = fd.rname.substr(0, fd.rname.size()-1);

          // Fall on sword if we cannot get data member type name

          if ( fd.rname == "" )
            throw cms::Exception("datamemberTypeFailure")
              << " can't get type for data member "
              << fd.classname << "::"
              << fd.expression << endl;

          if ( DBFunctionMember )
            cout << "       - datamember type (confirmation): " 
                 << fd.rname << endl;
        }
      else
        {
          //-------------------------------------------------------------------
          // This seems to be a method
          //-------------------------------------------------------------------
          if ( DBFunctionMember )
            cout << "       - is " << RED << "method " 
                 << DEFAULT_COLOR << endl;
          
          // Decode method and return a Reflex model of it in fd.method

          rfx::decodeMethod(fd);

         // Fall on sword if we did not find a valid method

          if ( !rfx::memberValid(fd.method) )
            throw cms::Exception("decodeMethodFailure")
              << " can't decode method: " 
              << fd.classname << "::" 
              << fd.expression << endl;

          // We have a valid method so get a model of its return type
          // Note: again, allow for the possibility that the value can be
          // returned by value, pointer or reference.

          fd.rtype   = fd.method.TypeOf().ReturnType().FinalType();
          fd.simple  = fd.rtype.IsFundamental();
          fd.pointer = fd.rtype.IsPointer();
          fd.reference = fd.rtype.IsReference();
          if ( fd.pointer ) fd.rtype = fd.rtype.ToType();

          // Get type name of returned object

          fd.rname = fd.rtype.Name(SCOPED+FINAL);
          if ( fd.rname == "" )
            throw cms::Exception("returnTypeFailure")
              << " can't get return type for method " 
              << fd.method.Name() << endl;

          
          if ( DBFunctionMember )
            cout << "       - return type: " << fd.rname << endl;

          // This could be an isAvailable method

          boost::regex aregex("^isAvailable[(]");
          boost::smatch amatch;
          fd.isAvailable = boost::regex_search(fd.expression, amatch, aregex);

          // This method could be an isNull method

          boost::regex nregex("^isNull[(]");
          boost::smatch nmatch;
          fd.isNull = boost::regex_search(fd.expression, nmatch, nregex);
        }

      //-----------------------------------------------------------------------
      // We have a valid method or data member. 
      //-----------------------------------------------------------------------

      // The return type or data member could be a smart pointer
      if ( !fd.simple)
        {
          Member m = rfx::getisNull(fd.method);
          fd.smartpointer = rfx::memberValid(m);
        }

      if ( fd.smartpointer )
        {
          // The data member or the return type is a smart pointer, so
          // insert a call to isAvailable()

          expr2 = string("isAvailable()") + delim + expr2;

          if ( DBFunctionMember )
            cout << "       - return type: " 
                 << RED << "smart pointer" 
                 << DEFAULT_COLOR << endl;
        }
      else if ( fd.isAvailable )
        {
          // This is an isAvailable method, so insert a call to isNull

          fd.rname = fd.classname; // same classname as current smart pointer
          expr2 = string("isNull()") + delim + expr2;
        }
      else if ( fd.isNull )
        {
          // This is an isNull method, so insert a call to get

          fd.rname = fd.classname; // same classname as current smart pointer
          expr2 = string("get()") + delim + expr2;
        }

      // Memory is needed by Reflex to store the return values from functions.
      // We need to reserve the right amount of space for each object
      // returned, which could of course be a fundamental (that is, simple)
      // type. We free all reserved memory in FunctionMember's destructor.

      fd.robject = Object(fd.rtype, fd.rtype.Allocate());

      // set return type code

      fd.rcode = Tools::FundamentalType(fd.rtype);
      if ( DBFunctionMember )
           cout << "       - return code: " 
                 << RED << fd.rcode 
                 << DEFAULT_COLOR << endl;

      // If the return type is simple, then we need to break out of this
      // loop because the analysis of the method is complete. However, if
      // the method is either isAvailable or isNull we must continue.
            
      if ( fd.simple )
        {
          if ( !fd.isAvailable )
            {
              if ( !fd.isNull )
                {
                  // This FunctionMember should always arrive here!
                  done = true;
                  if ( DBFunctionMember )
                    cout << "END FunctionMember - " 
                         << classname_ + "::" << expression_ << endl << endl;
                  break;
                }
            }
        }

      // The return type is not simple or the method is either isAvailable or
      // isNull. We therefore, need to continue: the 2nd part of the compound 
      // method becomes the expression on the next round and the return 
      // type becomes the next classname.
 
      expression = expr2;
      classname  = fd.rname;
    }

  if ( ! done )
    throw cms::Exception("FunctionMemberFailure")
      << " **** I can't understand this method: " 
      << classname_ << "::" << expression_ << endl
      << " **** make sure it returns a simple type"
      << endl;
}

bool 
FunctionMember::doNotCall(FunctionDescriptor& fd)
{
  const edm::Event* event = CurrentEvent::instance().get();
  if ( event == 0 ) return false;
  int run = event->id().run();
  if ( FunctionMember::donotcall.find(run) != 
       FunctionMember::donotcall.end() )
    {
      if ( FunctionMember::donotcall[run].find(fd.rname) !=
           FunctionMember::donotcall[run].end() )

        if ( DBFunctionMember )
          cout << "==> Skipping method " 
               << RED 
               << classname_ << "::" 
               << expression_
               << DEFAULT_COLOR
               << endl;
      return true;
    }
  return false;
}

void
FunctionMember::updatedoNotCall(FunctionDescriptor& fd)
{
  const edm::Event* event = CurrentEvent::instance().get();
  if ( event == 0 ) return;
  int run = event->id().run();
  if ( FunctionMember::donotcall.find(run) == 
       FunctionMember::donotcall.end() )
    {
      FunctionMember::donotcall[run] = map<string, int>();
    }
  FunctionMember::donotcall[run][fd.rname] = 0;
  if ( DBFunctionMember )
    cout << "==> Adding " << RED << fd.rname << DEFAULT_COLOR 
         << " to doNotCall list" << endl;
}

double
FunctionMember::invoke(void* address)
{
#ifdef DEBUG
  if ( DBFunctionMember )
    cout << "BEGIN FunctionMember::invoke" << endl;
#endif

  value_ = 0;
  longvalue_ = 0;
  void*  raddr = 0;

  // Loop over each part of method

  for(unsigned int depth=0; depth < fd_.size(); ++depth)
    {
      FunctionDescriptor& fd = fd_[depth]; // NB: get an alias NOT a copy!

//       // Check the return type of current method. If it points to an object
//       // that is on the doNotCall list for the current run, then don't call
//       // this method. The assumption is that if a collection is missing
//       // it is missing for the entire run.

//       if ( fd.pointer || fd.smartpointer )
//         {
//           if ( doNotCall(fd) ) 
//             {
//               raddr = 0;
//               value_ = 0;
//               longvalue_ = 0;
//               break;
//             }
//         }

      execute(fd, address, raddr, value_, longvalue_);

      if ( fd.simple )
        {
          //---------------------------------------
          // Fundamental return type
          //---------------------------------------
          // This is a fundamental type returned from
          // either a regular method or:
          // 1. a bool from the isAvailable() method of a smart pointer
          // 2. a bool from the isNull() method of a smart pointer
#ifdef DEBUG
          if ( DBFunctionMember )
            cout << "\tFunctionMember::invoke - FUNCTION:     " 
                 << BLUE << fd.method.Name() << DEFAULT_COLOR << endl
                 << "\t                       - RETURN TYPE:  " 
                 << RED << "FUNDAMENTAL" << DEFAULT_COLOR << endl
                 << "\t                       - RETURN VALUE: "
                 << RED
                 << value_
                 << DEFAULT_COLOR << endl;
#endif
          // This could be an isAvailable method. If so,
          // check its return value

          if ( fd.isAvailable )
            {
              bool available = (bool)value_;
              if ( available )
                {
#ifdef DEBUG
                  if ( DBFunctionMember )
                    cout << "\tFunctionMember::invoke - isAvailable returns: " 
                         << RED << "TRUE" << DEFAULT_COLOR << endl;
#endif
                }
              else
                {
                  // The collection is not available, so return a null pointer
                  edm::LogWarning("CollectionNotFound") << "\t" 
                                                        << classname_ 
                                                        << "::" 
                                                        << expression_ 
                                                        << endl;  
                  value_ = 0;
                  //updatedoNotCall(fd);
                  break; // break out of loop
                }
            }

          // This could be an isNull method. If so, check its return value

          else if ( fd.isNull )
            {
              bool null = (bool)value_;
              if ( null )
                {
                  // The collection is not available, so return a null pointer
                  edm::LogWarning("NullSmartPointer") << "\t" 
                                                      << classname_ 
                                                      << "::" 
                                                      << expression_ 
                                                      << endl;  
                  value_ = 0;
                  break; // break out of loop
                }
              else
                {
#ifdef DEBUG
                  if ( DBFunctionMember )
                    cout << "\tFunctionMember::invoke - isNull returns: " 
                         << RED << "FALSE" << DEFAULT_COLOR << endl;
#endif
                }
            }
        }
      else 
        {
          //---------------------------------------
          // Non-fundamental return type
          //---------------------------------------
#ifdef DEBUG
          if ( DBFunctionMember )
            cout << "\tFunctionMember::invoke - FUNCTION:       " 
                 << BLUE << fd.method.Name() << DEFAULT_COLOR << endl
                 << "\t                       - RETURN TYPE:    " 
                 << RED << "NON-FUNDAMENTAL" << DEFAULT_COLOR << endl
                 << "\t                       - RETURN ADDRESS: "
                 << RED
                 << raddr
                 << DEFAULT_COLOR << endl;
#endif
          if ( fd.pointer )
            {
              if ( raddr == 0 )
                {
                  edm::LogWarning("NullPointer") << "\t" 
                                                 << classname_ 
                                                 << "::" 
                                                 << expression_ 
                                                 << endl; 
                  value_ = 0;
                  break; // break out of loop
                }
            }

          // Return address becomes object address in next call
          address = raddr;
        }
    }
#ifdef DEBUG
  if ( DBFunctionMember )
    cout << "END FunctionMember::invoke" << endl << endl;
#endif

  raddr_ = raddr;
  return value_;
}

void* 
FunctionMember::raddress() { return raddr_; }

void
FunctionMember::execute(FunctionDescriptor& fd, 
                        void*   address, 
                        void*&  raddr,
                        double& value,
                        long double& longvalue)
{
  // classname   the parent class to which method/data member belongs
  // address     address of object whose method/data member is being called
  // method      object that models a method or a data member
  // args_       the arguments of the method to be called
  
  
#ifdef DEBUG
      if ( DBFunctionMember )
        cout << "\tFunctionMember::execute: " 
             << fd.classname << "::" 
             << fd.expression << endl;
#endif

  if ( fd.datamember )
    raddr = rfx::datamemberValue(fd.classname, address, fd.expression);
  else
    raddr = rfx::invokeMethod(fd, address);

  // If address is zero, bail out

  if ( raddr == 0 ) return;

  // If the function does not return a fundamental type then just return

  if ( fd.rcode == kNOTFUNDAMENTAL ) return;

  // Ok the function's return type is fundamental, so map it to a double
  // or a long double

  switch( fd.rcode )
    {
      // most common fundamental types
    case kDOUBLE:
      value = *static_cast<double*>(raddr);
      longvalue = value;
      break;
	
    case kFLOAT:
      value = static_cast<double>(*static_cast<float*>(raddr));
      longvalue = value;
      break;
      
    case kINT:
      value = static_cast<double>(*static_cast<int*>(raddr));
      longvalue = value;
      break;	  
      
    case kUNSIGNED_INT:
      value = static_cast<double>(*static_cast<unsigned int*>(raddr));
      longvalue = value;
      break;	
      
    case kUNSIGNED_SHORT_INT:
      value = static_cast<double>(*static_cast<unsigned short int*>(raddr));
      longvalue = value;
      break;
      
    case kBOOL:
      value = static_cast<double>(*static_cast<bool*>(raddr));
      longvalue = value;
      break;	    

    case kUNSIGNED_LONG_INT:
      value = static_cast<double>(*static_cast<unsigned long int*>(raddr));
      longvalue = value;
      break;

      // less common simple types

    case kCHAR:
      value = static_cast<double>(*static_cast<char*>(raddr));
      longvalue = value;
      break;
      
    case kSIGNED_CHAR:
      value = static_cast<double>(*static_cast<unsigned char*>(raddr));
      longvalue = value;
      break;
    
    case kSHORT_INT:
      value = static_cast<double>(*static_cast<short int*>(raddr));
      longvalue = value;
      break;	    
    
    case kLONG_INT:
      value = static_cast<double>(*static_cast<long int*>(raddr));
      longvalue = value;
      break;
    
    case kUNSIGNED_CHAR:
      value = static_cast<double>(*static_cast<unsigned char*>(raddr));
      longvalue = value;
      break;
      
      // long longs
      
    case kULONGLONG:
      longvalue = static_cast<long double>
        (*static_cast<unsigned long long*>(raddr));
      break;
    
    case kLONG_DOUBLE:
      longvalue = *static_cast<long double*>(raddr);
      break;	
    case kLONGLONG:
      longvalue = static_cast<long double>(*static_cast<long long*>(raddr));
      break;
        
    default:
      // Should never get here!
      edm::LogWarning("SHOULD_NEVER_GET_HERE") << "\t" 
                                               << fd.classname 
                                               << "::" 
                                               << fd.expression 
                                               << endl; 
    }
}

long double
FunctionMember::invokeLong(void* address) 
{
  invoke(address);
  return longvalue_; 
}

double
FunctionMember::operator()(void* address) { return invoke(address); }

std::string
FunctionMember::str() const
{
  ostringstream os;

  os << classname_ << "::" << expression_ << endl;

  for(unsigned int depth=0; depth < fd_.size(); ++depth)
    {
      const FunctionDescriptor& fd = fd_[depth];
      os << "  " << depth << endl;
      os << "\tclassname:  " << fd.classname << endl;
      os << "\texpression: " << fd.expression << endl;
      if ( fd.values.size() > (unsigned)0 )
        {
          os << "\t\tvalues: " << endl;
          for(unsigned i=0; i < fd.values.size(); i++)
            {
              os << "\t\t" 
                 << i 
                 << "  " << fd.args[i]
                 << ": " << fd.values[i]->str()  << endl;
            }
        }
    }
  return os.str();
}


std::ostream&
operator<<(std::ostream& os, const FunctionMember& o)
{
  os << o.str();
  return os;
}


