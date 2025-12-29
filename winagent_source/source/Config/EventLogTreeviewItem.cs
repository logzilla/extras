/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace SyslogAgent.Config
{
    public class EventLogTreeviewItem : BaseInpc
    {
        public string Name { get; set; }
        public EventLogTreeviewItem Parent { get; set; }
        public ObservableCollection<EventLogTreeviewItem> Children { get; set; }
        protected bool? is_checked_;
        public bool? IsChecked { get => is_checked_; set => Set(ref is_checked_, value); }
        public bool IsExpanded { get; set; }
        public string LeafPath { get; set; }

        public EventLogTreeviewItem()
        {
            if (Children == null)
            {
                Children = new ObservableCollection<EventLogTreeviewItem>();
            }
        }

        public void SetIsCheckedAll(bool? value)
        {
            foreach (var child in Children)
            {
                child.SetIsCheckedAll(value);
            }
            is_checked_ = value;
        }

        public EventLogTreeviewItem AddChild(string name)
        {
            var child = new EventLogTreeviewItem() { Name = name, Parent = this };
            Children.Add(child);
            return child;
        }

        protected void ResetCheckedFromChild()
        {
            Debug.WriteLine("{0} ResetCheckedFromChild", Name);
            int count = 0;
            int num_checked = 0;
            int num_unchecked = 0;
            foreach (var child in Children)
            {
                ++count;
                Debug.WriteLine("   {0} == {1}", child.Name, child.IsChecked);
                if (child.IsChecked == true)
                {
                    ++num_checked;
                }
                else if (child.IsChecked == false)
                {
                    ++num_unchecked;
                }
            }
            if (count == 0)
                return;
            if (num_checked == 0 && num_unchecked == count)
            {
                Debug.WriteLine("      {0} setting to false", Name);
                IsChecked = false;
            }
            else if (num_checked == count)
            {
                Debug.WriteLine("      {0} setting to true", Name);
                IsChecked = true;
            }
            else
            {
                Debug.WriteLine("      {0} setting to null", Name);
                IsChecked = null;
            }
            if (Parent != null)
            {
                Parent.ResetCheckedFromChild();
            }
            //is_checked_ = null;
        }

        protected override void OnPropertyChanged(string propertyName, object oldValue, 
            object newValue) {
            if (oldValue == newValue)
                return;
            if (propertyName == "IsChecked")
            {
                Debug.WriteLine("{0} -> {1} ({2})", Name, newValue, DateTime.Now);
                if (newValue == null)
                    return;

                foreach (var child in Children)
                {
                    child.IsChecked = (bool) newValue;
                }
                if (Parent != null)
                {
                    Parent.ResetCheckedFromChild();
                }
                Debug.WriteLine("{0} OnPropertyChanged done ({1})", Name, DateTime.Now);
            }
        }
    }
}

