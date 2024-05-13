/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace SyslogAgent.Config
{
    class EventLogTreeViewItemHelper : DependencyObject
    {
        public static readonly DependencyProperty IsCheckedProperty
            = DependencyProperty.RegisterAttached("IsChecked", typeof(bool?),
                typeof(EventLogTreeViewItemHelper),
                new PropertyMetadata(false, new PropertyChangedCallback(OnIsCheckedPropertyChanged)));

        private static void OnIsCheckedPropertyChanged(DependencyObject d, 
            DependencyPropertyChangedEventArgs e)
        {
            if (d is EventLogGroupMember && ((bool?)e.NewValue).HasValue)
                foreach (EventLogGroupMember p in (d as EventLogGroupMember).ChildMembers)
                    EventLogTreeViewItemHelper.SetIsChecked(p, (bool?)e.NewValue);

            if (d is EventLogGroupMember)
            {
                int num_checked 
                    = ((d as EventLogGroupMember).GetValue(EventLogTreeViewItemHelper.ParentProperty) 
                    as EventLogGroupMember)
                    .ChildMembers.Where(x => EventLogTreeViewItemHelper.GetIsChecked(x) == true).Count();
                int num_unchecked 
                    = ((d as EventLogGroupMember).GetValue(EventLogTreeViewItemHelper.ParentProperty) 
                    as EventLogGroupMember)
                    .ChildMembers.Where(x => EventLogTreeViewItemHelper.GetIsChecked(x) == false).Count();
                if (num_unchecked > 0 && num_checked > 0)
                {
                    EventLogTreeViewItemHelper.SetIsChecked((d as EventLogGroupMember)
                        .GetValue(EventLogTreeViewItemHelper.ParentProperty) as EventLogGroupMember, null);
                    return;
                }
                if (num_checked > 0)
                {
                    EventLogTreeViewItemHelper.SetIsChecked((d as EventLogGroupMember)
                        .GetValue(EventLogTreeViewItemHelper.ParentProperty) as EventLogGroupMember, true);
                    return;
                }
                EventLogTreeViewItemHelper.SetIsChecked((d as EventLogGroupMember)
                    .GetValue(EventLogTreeViewItemHelper.ParentProperty) as EventLogGroupMember, false);
            }
        }

        public static void SetIsChecked(EventLogGroupMember member, bool? IsChecked)
        {
            var element = member as DependencyObject;
            element.SetValue(EventLogTreeViewItemHelper.IsCheckedProperty, IsChecked);
            foreach (var child in member.ChildMembers)
                SetIsChecked(child, IsChecked);
        }
        public static bool? GetIsChecked(EventLogGroupMember member)
        {
            var element = member as DependencyObject;
            return (bool?)element.GetValue(EventLogTreeViewItemHelper.IsCheckedProperty);
        }

        public static readonly DependencyProperty ParentProperty 
            = DependencyProperty.RegisterAttached("Parent", typeof(object), 
                typeof(EventLogTreeViewItemHelper));
        public static void SetParent(DependencyObject element, object Parent)
        {
            element.SetValue(EventLogTreeViewItemHelper.ParentProperty, Parent);
        }
        public static object GetParent(DependencyObject element)
        {
            return (object)element.GetValue(EventLogTreeViewItemHelper.ParentProperty);
        }
    }
}
